#include "comment.h"

struct sockaddr_in getAddr(char *pszIp, uint16_t port) // 输入IP和端口，返回一个地址结构体
{
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;              // IPV4地址族
    addr.sin_addr.s_addr = strToInt(pszIp); // IP地址
    addr.sin_port = htons(port);            // 端口号，要转成大端字节序
    return addr;
}

int fd = 0;                    // 全局套接字
struct sockaddr_in addr;       // 服务器地址
char loginUserName[128] = {0}; // 当前登录用户的名字
void borrow_books();
void *work(void *p) //线程的任务
{
    while (1) // 不停的接受消息
    {
        MSG msg;
        read(fd, &msg, sizeof(msg)); // 读取服务器发来的消息
        if (msg.type == 4)           // 收到群聊消息
        {
            printf("%s : %s\n", msg.name, msg.buf); //收到群聊消息的时候将发送者的用户名进行显示，以便分清消息来源
        }
        else if (msg.type == 3) //收到服务器发来的私聊消息
        {
            SIG_CHAT *sc = (SIG_CHAT *)msg.buf; //收到服务器转发的别的的私聊结构体，里面有别的名字
            printf("From %s :%s\n", sc->selfName, sc->text);
        }
    }
}

void _register() //注册
{
    printf("请输入用户名和密码\n");
    REG_NODE rn;
    scanf("%s%s", rn.name, rn.password);
    MSG msg; // 定义一艘轮船
    // 把数据放入轮船
    msg.type = 1;                     // 轮船的运输类型为注册
    memcpy(msg.buf, &rn, sizeof(rn)); // 把注册结构体拷贝到轮船的仓库中
    write(fd, &msg, sizeof(msg));
    read(fd, &msg, sizeof(msg));
    printf("%s\n", msg.buf);
}

int login() // 登录
{
    printf("请输入用户名和密码\n");
    REG_NODE rn; // 这个注册结构体是用来登录的
    scanf("%s%s", rn.name, rn.password);
    MSG msg; // 定义一艘轮船
    // 把数据放入轮船
    msg.type = 2;                     // 轮船的运输类型为注册
    memcpy(msg.buf, &rn, sizeof(rn)); // 把注册结构体拷贝到轮船的仓库中
    write(fd, &msg, sizeof(msg));     // 把登录信息发给服务器

    // 接受服务器返回的消息
    read(fd, &msg, sizeof(msg)); // 继续用msg结构体变量来接受返回消息
    if (msg.type == 102)
    {
        printf("登录成功！\n");
        strcpy(loginUserName, rn.name); //保存登录用户的名字到全局变量中，其他函数可能需要用到
        return 1;
    }
    else if (msg.type == 1001)
    {
        printf("密码错误\n");
        return 0;
    }
}

int login_Root() //管理员登陆
{
    printf("请输入用户名和密码\n");
    REG_NODE rn; // 这个注册结构体是用来登录的
    scanf("%s%s", rn.name, rn.password);
    MSG msg; // 定义一艘轮船
    // 把数据放入轮船
    msg.type = 8;                     // 轮船的运输类型为注册
    memcpy(msg.buf, &rn, sizeof(rn)); // 把注册结构体拷贝到轮船的仓库中
    write(fd, &msg, sizeof(msg));     // 把登录信息发给服务器

    // 接受服务器返回的消息
    read(fd, &msg, sizeof(msg)); // 继续用msg结构体变量来接受返回消息
    if (msg.type == 102)
    {
        printf("登录成功！\n");
        strcpy(loginUserName, rn.name); //保存登录用户的名字到全局变量中，其他函数可能需要用到
        return 1;
    }
    else if (msg.type == 1001)
    {
        printf("密码错误\n");
        return 0;
    }
}

void change_password() //修改密码
{
    printf("输入你的旧密码:\n");
    MSG msg;
    msg.type = 6;
    scanf("%s", msg.buf);
    strcpy(msg.name, loginUserName);
    write(fd, &msg, sizeof(msg)); //将输入的旧密码与用户名传给服务器
    printf("已发送旧密码验证请求: 用户名=%s, 旧密码=%s\n", msg.name, msg.buf);

    read(fd, &msg, sizeof(msg));
    printf("%s\n", msg.buf);
    if (strcmp(msg.buf, "验证成功,请输入新密码") == 0) //验证成功即可输入新密码，否则返回
    {
        scanf("%s", msg.buf);
        write(fd, &msg, sizeof(msg));

        read(fd, &msg, sizeof(msg));
        printf("%s\n", msg.buf);
    }
    else
    {
        printf("旧密码错误\n");
    }
}

void groupChat() // 群聊
{
    pthread_t pt;
    pthread_create(&pt, NULL, work, NULL); // 创建一个用来接受聊天消息的线程
    while (1)
    {
        MSG msg;
        msg.type = 4;
        // printf("请输入聊天内容：\n");
        scanf("%s", msg.buf);
        strcpy(msg.name, loginUserName); //发送消息之前先将自己的用户名存入msg中，再一起发送给服务器进行转发
        if (strcmp(msg.buf, "quit") == 0)
        {
            pthread_cancel(pt); //取消线程
            break;
        }
        write(fd, &msg, sizeof(msg)); // 发送聊天消息给服务器
    }
}

void singleChat() // 私聊
{
    pthread_t pt;
    pthread_create(&pt, NULL, work, NULL); // 创建一个用来接受聊天消息的线程
    SIG_CHAT sc;                           //定义一个单聊结构体
    printf("请输入对方的名字\n");
    scanf("%s", sc.destName);           //输入对方的名字
    strcpy(sc.selfName, loginUserName); //当前登录的用户名
    while (1)
    {
        printf("请输入内容\n");
        scanf("%s", sc.text);
        if (strcmp(sc.text, "quit") == 0)
        {
            pthread_cancel(pt); //取消线程
            break;
        }
        MSG msg;
        msg.type = 3;
        memcpy(msg.buf, &sc, sizeof(SIG_CHAT)); //把私聊的数据拷贝的船舱（消息的Buf）
        write(fd, &msg, sizeof(MSG));           //把消息发送给服务器
    }
}

void select_books() //查询书籍信息
{
    MSG msg;
    msg.type = 11;
    printf("书籍信息如下\n"); //查询剩下书籍信息
    write(fd, &msg, sizeof(msg));
    read(fd, &msg, sizeof(msg));
    printf("%s\n", msg.buf);
}

void select_user() //查询用户信息
{
    MSG msg;
    msg.type = 13;
    printf("用户信息如下\n"); //查询剩下书籍信息
    write(fd, &msg, sizeof(msg));
    read(fd, &msg, sizeof(msg));
    printf("%s\n", msg.buf);
}

void change_user_book() //修改用户所借书籍信息
{
    select_user();
    MSG msg;
    msg.type = 14;
    printf("输入需要修改用户名称及修改后所借书籍名称(没有则输入0):\n");
    scanf("%s%s", msg.name, msg.buf);
    write(fd, &msg, sizeof(msg));
    read(fd, &msg, sizeof(msg));
    if (strcmp(msg.buf, "修改成功") == 0)
    {
        printf("修改成功\n");
    }
    else
    {
        printf("修改失败\n");
    }
}

void change_books() //修改书籍的信息
{

    MSG msg;
    msg.type = 12;
    printf("输入需要修改书籍名称及增加的书籍数量:\n");
    scanf("%s%s", msg.name, msg.buf);
    write(fd, &msg, sizeof(msg));
    read(fd, &msg, sizeof(msg));
    if (strcmp(msg.buf, "成功") == 0)
    {
        printf("修改书籍信息成功\n");
    }
    else
    {
        printf("修改书籍信息失败\n");
    }
}

void add_books() //添加书籍
{
    MSG msg;
    msg.type = 9;
    printf("输入书籍名称与数量\n");
    scanf("%s%s", msg.name, msg.buf);
    write(fd, &msg, sizeof(msg));
    read(fd, &msg, sizeof(msg));
    if (strcmp(msg.buf, "存储成功") == 0)
    {
        printf("存入书籍信息成功\n");
    }
    else
    {
        printf("存入书籍信息失败\n");
    }
}

void del_books() //删除书籍
{
    MSG msg;
    msg.type = 10;
    printf("输入书籍名称\n");
    scanf("%s", msg.name);
    write(fd, &msg, sizeof(msg));
    read(fd, &msg, sizeof(msg));
    if (strcmp(msg.buf, "删除成功") == 0)
    {
        printf("删除书籍成功\n");
    }
    else
    {
        printf("删除书籍失败\n");
    }
}

void del_user() //删除用户
{
    MSG msg;
    msg.type = 15;
    printf("输入用户名称\n");
    scanf("%s", msg.name);
    write(fd, &msg, sizeof(msg));
    read(fd, &msg, sizeof(msg));
    if (strcmp(msg.buf, "删除成功") == 0)
    {
        printf("删除用户成功\n");
    }
    else
    {
        printf("删除用户失败\n");
    }
}

void borrow_books() //借阅书籍
{
    MSG msg;
    msg.type = 16;
    printf("输入要借的书名:\n");
    strcpy(msg.name, loginUserName);
    scanf("%s", msg.buf);
    write(fd, &msg, sizeof(msg));
    read(fd, &msg, sizeof(msg));
    if (strcmp(msg.buf, "成功") == 0)
    {
        printf("借书成功\n");
    }
    else
    {
        printf("%s\n", msg.buf);
    }
}

void back_books() //归还书籍
{
    MSG msg;
    msg.type = 17;
    strcpy(msg.name, loginUserName);
    write(fd, &msg, sizeof(msg));
    read(fd, &msg, sizeof(msg));
    if (strcmp(msg.buf, "成功") == 0)
    {
        printf("还书成功\n");
    }
    else
    {
        printf("还书失败\n");
    }
}

void find_password()
{
    MSG msg;
    msg.type = 18;
    printf("输入用户名\n");
    scanf("%s", msg.buf);
    write(fd, &msg, sizeof(msg));
    read(fd, &msg, sizeof(msg));
    printf("%s\n", msg.buf);
}

int main(int len, char *arg[])
{
    addr = getAddr("192.168.50.199", 9000); // 获得一个地址结构体

    fd = socket(AF_INET, SOCK_STREAM, 0); // 创建了一个TCP的套接字（用来通信）

    if (-1 == connect(fd, (struct sockaddr *)&addr, sizeof(addr)))
    {
        printf("是不是没有开启服务器？\n");
        return 0;
    }

    while (1)
    {
        printf("1.注册 2.登录 3.找回密码 4.管理员登陆 5.退出系统 \n");
        int sel = -1;
        scanf("%d", &sel);
        switch (sel)
        {
        case 1:
            _register();
            break;
        case 2:
        {
            if (1 == login())
            {
                int x = 1;
                while (x) // 下一级菜单的循环
                {
                    printf("1.私聊 2.和在线用户群聊 3.查询图书 4.借阅书籍 5.归还书籍 6.修改密码 7.返回上一级\n");
                    int sel1 = -1;
                    scanf("%d", &sel1);
                    switch (sel1)
                    {
                    case 1:
                        singleChat();
                        break;
                    case 2:
                        groupChat();
                        break;
                    case 3:
                        select_books();
                        break;
                    case 4:
                        borrow_books();
                        break;
                    case 5:
                        back_books();
                        break;
                    case 6:
                        change_password();
                        break;
                    case 7:
                        x = 0;
                        break;
                    default:
                        break;
                    }
                }
            }
            break;
        }
        case 3:
            find_password();
            break;
        case 4:
            if (1 == login_Root())
            {
                int x = 1;
                while (x)
                {
                    printf("1.群聊 2.私聊 3.管理用户 4.管理书籍 5.返回上一级\n");
                    int val = -1;
                    scanf("%d", &val);
                    switch (val)
                    {
                    case 1:
                        groupChat();
                        break;
                    case 2:
                        singleChat();
                        break;
                    case 3:
                        printf("1.修改用户所借书目 2.删除用户 3.返回上一级 \n");
                        int p = -1;
                        scanf("%d", &p);
                        switch (p)
                        {
                        case 1:
                            change_user_book();
                            break;
                        case 2:
                            del_user();
                            break;

                        default:
                            break;
                        }
                        break;
                    case 4:
                        printf("1.修改书籍信息 2.增加书籍 3.删除书籍 4.查询书籍\n");
                        int dp = -1;
                        scanf("%d", &dp);
                        switch (dp)
                        {
                        case 1:
                            change_books();
                            break;
                        case 2:
                            add_books();
                            break;
                        case 3:
                            del_books();
                            break;
                        case 4:
                            select_books();
                            break;
                        default:
                            break;
                        }
                        break;
                    case 5:
                        x = 0;
                        break;
                    default:
                        break;
                    }
                }
            }
            break;
        case 5:
            exit(0);
            break;
        default:
            break;
        }
    }

    close(fd);

    return 0;
}
