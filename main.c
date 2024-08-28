#include "comment.h"

LOGIN_INFO fdArr[1000];                    //用于存储当前登陆用户的fd，用于私聊与群聊中
int fdCount = 0;                           //记录登陆的用户数量
struct epoll_event ev, events[MAX_EVENTS]; // 一个事件结构体，需要填写里面的事件和套接字描述符
int listen_sock, conn_sock, nfds, epollfd;
int fd, n;
sqlite3 *db;
typedef struct QueryNode
{
    char **ppResult; //结果
    int row;         //行
    int col;         //列
} QN;

QN get_table(sqlite3 *db, char *pSql) //用于获取sql语句的结果
{
    QN qn;
    char *msg = 0;
    int data = sqlite3_get_table(db, pSql, &qn.ppResult, &qn.row, &qn.col, &msg);
    if (data < 0)
    {
        printf("%s\n", msg);
    }
    return qn;
}

void excut_sql(sqlite3 *db, char *pSql) //执行sql语句
{
    char *msg = 0;
    sqlite3_exec(db, pSql, 0, 0, &msg);
    if (msg != 0)
    {
        printf("%s\n", msg);
    }
}

void printQN(QN qn) //将qn中的结果打印出来
{
    for (size_t i = qn.col; i < qn.row * qn.col + qn.col; i++)
    {
        printf("%s\n", qn.ppResult[i]);
    }
}

struct sockaddr_in getAddr(char *path, unsigned int port) //创建一个地址结构体
{
    struct sockaddr_in addr;
    addr.sin_addr.s_addr = strToInt(path);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    return addr;
}

void open_db_user(sqlite3 **db, char *table_name) //创建user表，用于用户登陆
{
    int data = -1;

    //打开指定的数据库文件，如果不存在将创建一个同名的数据库文件

    data = sqlite3_open(table_name, db);
    if (data < 0)
    {
        printf("create testsqlite failure:%s\n", sqlite3_errmsg(*db)); //创建失败
    }
    else
    {
        // printf("create testsqlite successfuly\n"); //创建成功
        char buf[128];
        sprintf(buf, "CREATE TABLE IF NOT EXISTS user (name varchar(255),password varchar(255),borrow varchar(255));");
        excut_sql(*db, buf);
    }
}

void open_db_root(sqlite3 **db, char *table_name)
{
    int data = -1;

    //打开指定的数据库文件，如果不存在将创建一个同名的数据库文件

    data = sqlite3_open(table_name, db);
    if (data < 0)
    {
        printf("create testsqlite failure"); //创建失败
    }
    else
    {
        // printf("create testsqlite successfuly\n"); //创建成功
        char buf[128];
        sprintf(buf, "CREATE TABLE  root (name varchar(255),password varchar(255));");
        excut_sql(*db, buf);

        sprintf(buf, "SELECT * FROM root WHERE name='Orca';"); //查询管理员表中是否有名为‘Orca’的名字，没有则创建一个，作为初始管理员
        excut_sql(*db, buf);
        QN qn = get_table(*db, buf);
        if (qn.row == 0)
        {
            printf("qn.row:%d\n", qn.row);
            sprintf(buf, "INSERT INTO root VALUES('Orca','1234')");
            excut_sql(*db, buf);
        }
    }
}

void init_Root() //初始化管理员名单，只执行一次，之后不需要再执行
{
    sqlite3 *db_root = NULL;
    open_db_root(&db_root, "root.db");
    sqlite3_close(db_root);
}

void open_db_books(sqlite3 **db, char *table_name) //创建user表，用于用户登陆
{
    int data = -1;

    //打开指定的数据库文件，如果不存在将创建一个同名的数据库文件

    data = sqlite3_open(table_name, db);
    if (data < 0)
    {
        printf("create testsqlite failure:%s\n", sqlite3_errmsg(*db)); //创建失败
    }
    else
    {
        // printf("create testsqlite successfuly\n"); //创建成功
        char buf[128];
        sprintf(buf, "CREATE TABLE IF NOT EXISTS book (book_name varchar(255),num varchar(255));");
        excut_sql(*db, buf);
    }
}

void change_password(MSG msg) //用户修改自己的密码
{
    open_db_user(&db, "user.db");
    printf("用户想修改密码\n");
    printf("用户的fd:%d\n", events[n].data.fd);
    printf("msg.buf:%s msg.name:%s\n", msg.buf, msg.name); //打印出用户的基本信息，如fd，输入的用户名及密码
    char buf[512];
    sprintf(buf, "SELECT * FROM user WHERE name='%s' and password='%s';", msg.name, msg.buf); //查询所有姓名和密码与输入一致的人，有则验证成功
    excut_sql(db, buf);
    QN qn = get_table(db, buf);
    printf("qn.row:%d\n", qn.row);
    if (qn.row != 0)
    {
        MSG msg1;
        strcpy(msg1.buf, "验证成功,请输入新密码");
        write(events[n].data.fd, &msg1, sizeof(msg1)); //将验证成功的消息发回客户端

        read(events[n].data.fd, &msg1, sizeof(msg1)); //读取客户端发来的消息

        sprintf(buf, "UPDATE user SET password = '%s' WHERE name = '%s';", msg1.buf, msg.name); //根据读取的消息更新用户数据库
        excut_sql(db, buf);
        strcpy(msg1.buf, "密码修改成功\n");
        write(events[n].data.fd, &msg1, sizeof(msg1));
    }
    else
    {
        printf("密码错误\n");
        strcpy(msg.buf, "密码错误");
        write(events[n].data.fd, &msg, sizeof(msg));
    }
}

void select_books(MSG msg) //查询书籍数据库的所有消息
{

    sqlite3 *db_book = NULL;
    open_db_books(&db_book, "book.db");
    char buf[128];
    sprintf(buf, "SELECT * FROM book;");
    QN qn = get_table(db_book, buf); //qn.ppResult存有语句执行结果
    if (qn.row == 0)
    {
        strcpy(msg.buf, "No books found in the database.");
    }
    else
    {
        char result[2048] = {0}; // 用于存储查询结果
        int offset = 0;

        for (int i = 0; i < qn.row; i++)
        {
            //将结果存储在result中
            offset += snprintf(result + offset, sizeof(result) - offset, "Book Name: %s, Number: %s\n", qn.ppResult[(i + 1) * qn.col], qn.ppResult[(i + 1) * qn.col + 1]);
        }

        strncpy(msg.buf, result, sizeof(msg.buf) - 1);
        msg.buf[sizeof(msg.buf) - 1] = '\0'; // 确保字符串以空字符结尾
    }

    write(events[n].data.fd, &msg, sizeof(msg));
    sqlite3_close(db_book);
}

void select_user(MSG msg) //查询用户数据库的信息
{
    sqlite3 *db_user = NULL;
    open_db_user(&db_user, "user.db");
    char buf[128];
    sprintf(buf, "SELECT * FROM user;");
    QN qn = get_table(db_user, buf);
    if (qn.row == 0)
    {
        strcpy(msg.buf, "No users found in the database.");
    }
    else
    {
        char result[2048] = {0}; // 用于存储查询结果
        int offset = 0;

        for (int i = 0; i < qn.row; i++)
        {
            offset += snprintf(result + offset, sizeof(result) - offset, "User Name: %s, password: %s,borrow: %s\n", qn.ppResult[(i + 1) * qn.col], qn.ppResult[(i + 1) * qn.col + 1], qn.ppResult[(i + 1) * qn.col + 2]);
        }

        strncpy(msg.buf, result, sizeof(msg.buf) - 1);
        msg.buf[sizeof(msg.buf) - 1] = '\0'; // 确保字符串以空字符结尾
    }
    write(events[n].data.fd, &msg, sizeof(msg));
    sqlite3_close(db_user);
}

void change_books(MSG msg, int change) //修改书籍的保有量，在借阅书籍和管理员手动修改图书数量时用过
{
    sqlite3 *db_book = NULL;
    open_db_books(&db_book, "book.db");
    char buf[128];
    sprintf(buf, "SELECT num FROM book WHERE book_name = '%s';", msg.name); //根据书籍名称查询书籍数量
    printf("%s\n", buf);
    QN qn = get_table(db_book, buf);
    if (qn.row == 0)
    {
        strcpy(msg.buf, "Book not found");
    }
    else
    {
        int current_num = atoi(qn.ppResult[qn.col]); // 当前数量
        int new_num = current_num + change;          // 增加或减少数量
        if (new_num < 0)
        {
            strcpy(msg.buf, "输入有误"); // 确保数量不为负数
        }

        sprintf(buf, "UPDATE book SET num='%d' WHERE book_name = '%s';", new_num, msg.name);
        printf("%s\n", buf);
        excut_sql(db_book, buf);
        strcpy(msg.buf, "成功");
    }
    write(events[n].data.fd, &msg, sizeof(msg));
    sqlite3_close(db_book);
}

void add_books(MSG msg) //增加书籍，管理员登陆后可用
{
    sqlite3 *db_book = NULL;
    open_db_books(&db_book, "book.db");
    char buf[128];
    sprintf(buf, "INSERT INTO book VALUES('%s','%s');", msg.name, msg.buf); //在书籍数据库中插入一条新的数据
    excut_sql(db_book, buf);
    strcpy(msg.buf, "存储成功");
    write(events[n].data.fd, &msg, sizeof(msg));
    sqlite3_close(db_book);
}

void del_books(MSG msg) //删除书籍，管理员登陆后可用
{
    sqlite3 *db_book = NULL;
    open_db_books(&db_book, "book.db");
    char buf[128];
    sprintf(buf, "DELETE FROM book WHERE book_name = '%s';", msg.name);
    excut_sql(db_book, buf);
    strcpy(msg.buf, "删除成功");
    write(events[n].data.fd, &msg, sizeof(msg));
    sqlite3_close(db_book);
}

void registe_user(MSG msg) //用户注册
{
    open_db_user(&db, "user.db");
    REG_NODE *rn = (REG_NODE *)msg.buf;
    char buf[512];
    sprintf(buf, "SELECT * FROM user WHERE name='%s';", rn->name); //查询是否有相同用户名
    excut_sql(db, buf);
    QN qn = get_table(db, buf);

    if (qn.row != 0)
    {
        strcpy(msg.buf, "账号已注册"); //有相同用户名返回失败
        write(events[n].data.fd, &msg, sizeof(msg));
        return;
    }

    sprintf(buf, "INSERT INTO user VALUES('%s','%s','0')", rn->name, rn->password); //用户名可用则注册成功
    excut_sql(db, buf);

    strcpy(msg.buf, "账号注册成功!");
    write(events[n].data.fd, &msg, sizeof(msg));
    printf("注册成功了,用户名：%s 密码：%s\n", rn->name, rn->password);
}

void login_in(MSG msg) //登陆
{
    open_db_user(&db, "user.db");
    REG_NODE *rn = (REG_NODE *)msg.buf; //登录也是用的注册结构体，所以把仓库转为注册结构体类型

    char buf[512];
    sprintf(buf, "SELECT * FROM user WHERE name='%s' AND password='%s';", rn->name, rn->password);
    //文件里面的用户名和密码
    printf("rn->name:%s,rn->password:%s\n", rn->name, rn->password);
    excut_sql(db, buf);
    QN qnUser = get_table(db, buf);

    if (qnUser.row != 0)
    {
        msg.type = 102;                              //成功
        write(events[n].data.fd, &msg, sizeof(msg)); //回复客户端消息
        fdArr[fdCount].fd = events[n].data.fd;       //把通信套接字保存到数组中
        strcpy(fdArr[fdCount].name, rn->name);       //把登录用户的名字拷贝进去
        fdCount++;                                   //登录人数增加一个
        printf("用户登录成功，登录人数为%d\n", fdCount);
        return; //因为找到了登录用户，所以不需要继续往后找
    }

    if (msg.type != 102)
    {
        msg.type = 1001;                             //登录出错了
        write(events[n].data.fd, &msg, sizeof(msg)); //回复客户端消息
    }
}

void login_in_root(MSG msg) //管理员登陆
{
    sqlite3 *db_root = NULL;
    REG_NODE *rn = (REG_NODE *)msg.buf;
    open_db_root(&db_root, "root.db");
    char buf[512];
    sprintf(buf, "SELECT * FROM root WHERE name='%s' AND password='%s';", rn->name, rn->password);
    //文件里面的用户名和密码
    printf("rn->name:%s,rn->password:%s\n", rn->name, rn->password);
    excut_sql(db_root, buf);
    QN qnUser = get_table(db_root, buf);

    if (qnUser.row != 0)
    {
        msg.type = 102;                              //成功
        write(events[n].data.fd, &msg, sizeof(msg)); //回复客户端消息
        fdArr[fdCount].fd = events[n].data.fd;       //把通信套接字保存到数组中
        strcpy(fdArr[fdCount].name, rn->name);       //把登录用户的名字拷贝进去
        fdCount++;                                   //登录人数增加一个
        printf("管理员登陆成功\n");
        return; //因为找到了登录用户，所以不需要继续往后找
    }

    if (msg.type != 102)
    {
        msg.type = 1001;                             //登录出错了
        write(events[n].data.fd, &msg, sizeof(msg)); //回复客户端消息
    }
    sqlite3_close(db_root);
}

void change_user(MSG msg) //管理用户所借书籍
{
    sqlite3 *db_user = NULL;
    open_db_user(&db_user, "user.db");
    char buf[128];
    sprintf(buf, "UPDATE user SET borrow='%s' WHERE name = '%s';", msg.buf, msg.name); //将用户所借书籍修改为msg.buf所记录的数据
    excut_sql(db_user, buf);
    strcpy(msg.buf, "修改成功");
    write(events[n].data.fd, &msg, sizeof(msg));
    sqlite3_close(db_user);
}

void del_user(MSG msg) //删除用户
{
    sqlite3 *db_user = NULL;
    open_db_user(&db_user, "user.db");
    char buf[128];
    sprintf(buf, "DELETE FROM user WHERE name = '%s';", msg.name); //更新用户的数据库进行删除
    excut_sql(db_user, buf);
    strcpy(msg.buf, "删除成功");
    write(events[n].data.fd, &msg, sizeof(msg));
    sqlite3_close(db_user);
}

void borrow_books(MSG msg) //借阅书籍
{
    sqlite3 *db_user = NULL;
    open_db_user(&db_user, "user.db");
    char buf[128];
    sprintf(buf, "SELECT borrow FROM user WHERE name = '%s';", msg.name);
    QN qn = get_table(db_user, buf);
    if (strcmp(qn.ppResult[qn.col], "0") != 0) //判断用户的借阅部分是否为“0”，是则可以借书，否则先还书
    {
        printf("qn.col=%d,qn.ppResult[qn.col]=%s\n", qn.col, qn.ppResult[qn.col]);
        strcpy(msg.buf, "您已经借过书了,请先还书");
        write(events[n].data.fd, &msg, sizeof(msg));
    }
    else
    {
        sqlite3 *db_book = NULL;
        open_db_books(&db_book, "book.db");
        char buf[128];
        sprintf(buf, "SELECT * FROM book;");
        QN qn = get_table(db_book, buf);
        if (qn.row == 0)
        {
            strcpy(msg.buf, "No books found in the database.");
        }
        else
        {
            char result[2048] = {0}; // 用于存储查询结果
            int offset = 0;

            for (int i = 0; i < qn.row; i++)
            {
                offset += snprintf(result + offset, sizeof(result) - offset, "Book Name: %s, Number: %s\n", qn.ppResult[(i + 1) * qn.col], qn.ppResult[(i + 1) * qn.col + 1]);
            }
            int index = 0;
            for (size_t i = 0; i < qn.row; i++)
            {
                if (strcmp(msg.buf, qn.ppResult[(i + 1) * qn.col]) == 0)
                {
                    printf("msg.buf:%s ,qn.ppResult[(i + 1) * qn.col]:%s\n", msg.buf, qn.ppResult[(i + 1) * qn.col]);
                    index = 1;
                }
            }
            if (index == 1)
            {

                sprintf(buf, "UPDATE user SET borrow='%s' WHERE name = '%s';", msg.buf, msg.name);
                excut_sql(db_user, buf);
                MSG msg1;
                strcpy(msg1.name, msg.buf);
                change_books(msg1, -1);
            }
            else
            {
                sprintf(msg.buf, "没有这本书");
                write(events[n].data.fd, &msg, sizeof(msg));
            }
        }

        sqlite3_close(db_book);
    }
    sqlite3_close(db_user);
}

void back_books(MSG msg) //归还书籍
{
    sqlite3 *db_user = NULL;
    open_db_user(&db_user, "user.db");
    char buf[128];
    sprintf(buf, "SELECT borrow FROM user WHERE name = '%s';", msg.name);
    QN qn = get_table(db_user, buf);
    char *arr = qn.ppResult[qn.col];
    MSG msg1;
    strcpy(msg1.name, arr);
    change_books(msg1, 1);
    sprintf(buf, "UPDATE user SET borrow = '%s' WHERE name = '%s';", "0", msg.name);
    excut_sql(db_user, buf);
    sqlite3_close(db_user);
}

void find_password(MSG msg)
{
    sqlite3 *db_user = NULL;
    open_db_user(&db_user, "user.db");
    char buf[128];
    sprintf(buf, "SELECT password FROM user WHERE name = '%s';", msg.buf);
    QN qn = get_table(db_user, buf);
    if (qn.col != 0)
    {
        sprintf(msg.buf, "密码是:%s", qn.ppResult[qn.col]);
        write(events[n].data.fd, &msg, sizeof(msg));
    }
    else
    {
        sprintf(msg.buf, "输入的用户名不正确");
        write(events[n].data.fd, &msg, sizeof(msg));
    }
}

void work() //接收数据并处理
{
    for (;;)
    {
        // 等待eppll对象里面的套接字上发生结构体中指定的事件，并且把发生事件的结构体赋值到数组events里面
        // 同时会返回发生事件的套接字的数量
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if (nfds == -1)
        {
            perror("epoll_wait");
        }
        for (n = 0; n < nfds; ++n)
        {
            if (events[n].data.fd == fd) //当前监听套接字发送可读消息，有新的连接，将其加入events中
            {
                struct sockaddr_in destip;      //定义一个对方ip的地址结构体，用于存放对方的ip信息
                socklen_t len = sizeof(destip); //定义一个socklen_t 类型的长度，保存对方地址结构体的大小
                printf("监听套接字发生可读消息,有新连接接入\n");
                int newFd = accept(fd, (struct sockaddr *)&destip, &len); //接收新的连接，给定一个新的套接字描述符newFd用于接收
                if (newFd == -1)                                          //接收失败
                {
                    perror("accept");
                }

                ev.events = EPOLLIN;
                ev.data.fd = newFd;
                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, newFd, &ev) == -1) //把监听套接字以及套接字对应的事件结构体添加到epoll对象里
                {
                    perror("epoll_ctl: conn_sock");
                }
            }
            else
            {
                MSG msg;
                int ret = read(events[n].data.fd, &msg, sizeof(msg)); //读取由客户端发来的消息，并将读取的字节数存储在ret中
                if (ret == -1 || ret == 0)
                {
                    perror("套接字出错!");
                    ev.events = EPOLLIN; //感兴趣的事件是可读事件
                    ev.data.fd = events[n].data.fd;
                    epoll_ctl(epollfd, EPOLL_CTL_DEL, events[n].data.fd, &ev);
                }
                else
                {
                    if (msg.type == 1) //收到注册消息
                    {
                        registe_user(msg);
                    }
                    else if (msg.type == 2) //收到登录消息
                    {
                        login_in(msg);
                    }
                    else if (msg.type == 3) //收到私聊消息
                    {
                        SIG_CHAT *sc = (SIG_CHAT *)msg.buf;
                        for (size_t i = 0; i < fdCount; i++) //遍历素有登录的人，查找sc->destName，给他发消息
                        {
                            if (strcmp(sc->destName, fdArr[i].name) == 0) //判断登录数组中的元素的名字和收到的私聊信息中的目标的名字是否相同
                            {
                                write(fdArr[i].fd, &msg, sizeof(MSG));
                                break;
                            }
                        }
                    }
                    else if (msg.type == 4) //群聊消息
                    {
                        for (size_t i = 0; i < fdCount; i++)
                        {
                            if (fdArr[i].fd != events[n].data.fd) //将消息转发给除了自己的所有已登录用户
                            {
                                printf("把消息转发给fd:%d\n", fdArr[i].fd);
                                write(fdArr[i].fd, &msg, sizeof(msg));
                            }
                        }
                    }

                    else if (msg.type == 6) //修改密码
                    {
                        change_password(msg);
                    }
                    else if (msg.type == 8) //管理员登陆
                    {
                        login_in_root(msg);
                    }
                    else if (msg.type == 9) //用于增加书籍
                    {
                        add_books(msg);
                    }
                    else if (msg.type == 10) //用于删除书籍
                    {
                        del_books(msg);
                    }
                    else if (msg.type == 11) //查询书籍信息
                    {
                        select_books(msg);
                    }
                    else if (msg.type == 12) //用于修改书籍
                    {

                        change_books(msg, atoi(msg.buf));
                    }
                    else if (msg.type == 13) //查询用户信息
                    {
                        select_user(msg);
                    }
                    else if (msg.type == 14) //用于修改用户的借书情况
                    {
                        change_user(msg);
                    }
                    else if (msg.type == 15) //用于删除用户
                    {
                        del_user(msg);
                    }
                    else if (msg.type == 16) //用于用户借阅书籍
                    {
                        borrow_books(msg);
                    }
                    else if (msg.type == 17) //用于还书
                    {
                        back_books(msg);
                    }
                    else if (msg.type == 18) //找回密码
                    {
                        find_password(msg);
                    }
                }
            }
        }
    }
}

int main(int argc, char const *argv[])
{
    // init_Root();
    db = NULL;
    open_db_user(&db, "user.db");

    struct sockaddr_in addr = getAddr("192.168.50.199", 9000); //创建一个地址结构体，输入ip与端口号.
    fd = socket(AF_INET, SOCK_STREAM, 0);                      //创建套接字，fd是文件描述符.

    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)); //用来让服务器可以在短时间多次绑定同一个端口号.

    if (-1 == bind(fd, (struct sockaddr *)&addr, sizeof(addr))) //绑定端口号与ip地址.
    {
        perror("bind error");
        return 0;
    }

    listen(fd, 5); //开启等待队列，同时最多5个客户端连接.
    printf("最大fd为:%d\n", fd);

    /* Code to set up listening socket, 'listen_sock',
              (socket(), bind(), listen()) omitted */

    epollfd = epoll_create1(0); // 创建一个epoll对象
    if (epollfd == -1)
    {
        perror("epoll_create1");
    }

    ev.events = EPOLLIN;
    ev.data.fd = fd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev) == -1)
    {
        perror("epoll_ctl: listen_sock");
    }
    work(); //用于根据msg.type的值处理msg.buf

    close(fd);

    return 0;
}
