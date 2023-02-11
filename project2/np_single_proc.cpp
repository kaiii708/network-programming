#include <iostream>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/wait.h>

//新增
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <arpa/inet.h>

using namespace std;

#define MAX_SINGLE_INPUT_LENGTH 15000
#define MAX_COMMAND_LENGTH 256
#define MAX_PIPES_NUMBER 1000
#define MAX_COMMAND_NUMBER 5000
/////////////////
#define ENV_NUMBER 100
#define ENV_LENGTH 500
#define MAX_USERS_NUMBER 30
#define MAX_NAME_LENGTH 20

struct inputSegmentTable
{
    char *segmentation[MAX_SINGLE_INPUT_LENGTH];
    int length = 0;
    int index = 0;
};

enum pipeType
{
    none,
    ordinary,
    numbered,
    numberedWithError,
    // out_userpipe,
    in_userpipe
};

struct command
{
    int argc;
    char *argv[MAX_COMMAND_LENGTH];
    pipeType pipetype = none;
    int pipeNum = 0; //需要預設嗎
    char *filename = NULL;
    int from_user;
    int to_user;
    int UPtobeClear = -1;
    // int createpipetouser = -1;
    // int pipeoutfromuser = -1;
    bool out_userpipe = false;
};

struct pipeFD
{
    int FDInOut[2];
    int countDown = -1;
    bool isNumPipe = false;
};

struct userPipe
{
    int from_user;
    int to_user;
    int pipefd[2];
    bool isOccupied = false;
};

struct EnvTable
{
    //////////////////////
    char key[ENV_NUMBER][ENV_LENGTH];
    char val[ENV_NUMBER][ENV_LENGTH];
    int length;
};

struct userField
{
    char name[MAX_NAME_LENGTH];
    char ip_port[30];
    int sockfd;
    bool isLogin = false;
    EnvTable envTable;
    pipeFD pipeFDs[MAX_PIPES_NUMBER];
    int pipeLength = 0;
};

//全域變數
userField userTable[MAX_USERS_NUMBER + 1];
int numOfLoginUser = 0;

userPipe userPipes[MAX_PIPES_NUMBER];
int userPipesLength;
char raw_command[MAX_COMMAND_LENGTH];

void userTableInit(userField *userTable)
{
    for (int i = 1; i <= MAX_USERS_NUMBER; i++)
    {
        strcpy(userTable[i].name, "(no name)");
        userTable[i].isLogin = false;
    }
}

void envTableInit(EnvTable &env)
{
    env.length = 0;
    strcpy(env.key[env.length], "PATH");
    strcpy(env.val[env.length], "bin:.");
    env.length++;
}

void initInputTable(inputSegmentTable &inputTable)
{
    for (int i = 0; i < inputTable.length; i++)
    {
        memset(&inputTable.segmentation[i], '\0', sizeof(inputTable.segmentation[i]));
    }
    inputTable.length = 0;
    inputTable.index = 0;
}

void segmentInput(char *input, inputSegmentTable &inputTable)
{
    const char *d = " ";
    char *pch = strtok(input, d);
    while (pch != NULL)
    {
        inputTable.segmentation[inputTable.length++] = pch;
        pch = strtok(NULL, d);
    }
}

void initCommand(command &Command)
{
    Command.argc = 0;
    memset(Command.argv, '\0', sizeof(Command.argv));
    Command.pipetype = none;
    Command.pipeNum = 0; //需要預設嗎
    Command.filename = NULL;
    Command.UPtobeClear = -1;
    Command.from_user = -1;
    Command.to_user = -1;
    Command.out_userpipe = false;
    // Command.createpipetouser = -1;
    // Command.pipeoutfromuser = -1;
}

int getUserID(int sockfd)
{
    for (int id = 1; id <= MAX_USERS_NUMBER; id++)
    {
        if (userTable[id].sockfd == sockfd && userTable[id].isLogin)
        {
            return id;
        }
    }
    return -1;
}

//看要執行什麼command(看第一個，eg. ls | cat | cat)
//參數 cat test.html
//停的時候：{ 1.後面沒東西（eg. 上例的最後一個cat）
//          2.pipe |  |3  !2
//          3.file redirection (<),多read一個filename
//          }
// output: argv[], eg.cat text.txt
//                =>[0]=cat [1]text.txt
//        pipetype -none
//                 -ordinary
//                 -numbered
//                 -nuberedWithError
//        pipeNum, eg. |3 -> 3, !2 -> 2, 1 -> 1
//        filename: 若出現 >,>的下一個
void parseCommand(inputSegmentTable &inputTable, command &Command, int sockfd)
{
    int currentIndex = 0;

    while (inputTable.index < inputTable.length)
    {
        if (inputTable.segmentation[inputTable.index][0] != '<' && inputTable.segmentation[inputTable.index][0] != '|' && inputTable.segmentation[inputTable.index][0] != '>')
        {
            Command.argv[currentIndex++] = inputTable.segmentation[inputTable.index++];
            Command.argc++;
        }
        if (inputTable.segmentation[inputTable.index] == NULL)
        {
            break;
        }

        if (strcmp(inputTable.segmentation[inputTable.index], "|") == 0)
        {
            Command.pipetype = ordinary;
            Command.pipeNum = 1;
            inputTable.index++;
            break;
        }
        else if (inputTable.segmentation[inputTable.index][0] == '|')
        { //不確定指標陣列
            Command.pipetype = numbered;
            Command.pipeNum = atoi(&inputTable.segmentation[inputTable.index][1]);
            inputTable.index++;
            break;
        }
        else if (inputTable.segmentation[inputTable.index][0] == '!')
        {
            Command.pipetype = numberedWithError;
            Command.pipeNum = atoi(&inputTable.segmentation[inputTable.index][1]);
            inputTable.index++;
            break;
        }
        else if (strcmp(inputTable.segmentation[inputTable.index], ">") == 0)
        {

            Command.filename = inputTable.segmentation[++inputTable.index];
            inputTable.index++;
            break;
        }
        else if (inputTable.segmentation[inputTable.index][0] == '>')
        {
            // if (Command.out_userpipe == true)
            // {
            //     Command.pipeoutfromuser = Command.from_user;
            // }
            Command.pipetype = in_userpipe;
            Command.to_user = atoi(&inputTable.segmentation[inputTable.index][1]);
            // Command.from_user = getUserID(sockfd);
            inputTable.index++;
            if (inputTable.index < inputTable.length && inputTable.segmentation[inputTable.index][0] == '<')
            {
                continue;
            }
            break;
        }
        else if (inputTable.segmentation[inputTable.index][0] == '<')
        {
            // Command.pipetype = out_userpipe;
            // if (Command.pipetype == in_userpipe)
            // {
            //     Command.createpipetouser = Command.to_user;
            // }
            Command.out_userpipe = true;
            Command.from_user = atoi(&inputTable.segmentation[inputTable.index][1]);
            // Command.to_user = getUserID(sockfd);
            inputTable.index++;
        }
    }
}

void welcomeMsg(int sockfd)
{
    char message[] = "****************************************\n** Welcome to the information server. **\n****************************************\n";
    write(sockfd, message, strlen(message));
}

void broadcast(char *msg)
{
    int countdown = numOfLoginUser;
    for (int i = 1; i <= MAX_USERS_NUMBER; i++)
    {
        if (userTable[i].isLogin == true)
        {
            write(userTable[i].sockfd, msg, strlen(msg));
            if (--countdown == 0)
            {
                break;
            }
        }
    }
}

void userLogin(int ssock, sockaddr_in cl_addr)
{
    char loginMsg[2000];
    char port[10];
    int i;
    //找id並更新userTable
    for (i = 1; i <= MAX_USERS_NUMBER; i++)
    {
        if (userTable[i].isLogin == false)
        {
            userTable[i].sockfd = ssock;
            userTable[i].isLogin = true;
            sprintf(port, "%d", ntohs(cl_addr.sin_port));
            strcpy(userTable[i].ip_port, inet_ntoa(cl_addr.sin_addr));
            strcat(userTable[i].ip_port, ":");
            strcat(userTable[i].ip_port, port);
            envTableInit(userTable[i].envTable);
            numOfLoginUser++;
            break;
        }
    }
    welcomeMsg(ssock);
    sprintf(loginMsg, "*** User '%s' entered from %s. ***\n", userTable[i].name, userTable[i].ip_port);
    broadcast(loginMsg);
}

void clearUserPipeTable(int userID)
{
    for (int i = 0; i < MAX_PIPES_NUMBER; i++)
    {
        if (userPipes[i].from_user == userID || userPipes[i].to_user == userID)
        {
            userPipes[i].isOccupied = false;
        }
    }
}

void userLogout(fd_set &afds, int ssock)
{
    FD_CLR(ssock, &afds);
    char logoutMsg[2000];
    int index;
    index = getUserID(ssock);
    clearUserPipeTable(index);
    sprintf(logoutMsg, "*** User '%s' left. ***\n", userTable[index].name);
    userTable[index].isLogin = false;
    strcpy(userTable[index].name, "(no name)");
    numOfLoginUser--;
    close(ssock);
    broadcast(logoutMsg);
}

void who(int sockfd)
{
    char columnName[1000];
    char output[1000];
    strcpy(columnName, "<ID>\t<nickname>\t<IP:port>\t<indicate me>\n");
    write(sockfd, columnName, strlen(columnName));
    for (int i = 1; i <= MAX_USERS_NUMBER; i++)
    {
        if (userTable[i].isLogin)
        {
            if (userTable[i].sockfd == sockfd)
            {
                sprintf(output, "%d\t%s\t%s\t%s\n", i, userTable[i].name, userTable[i].ip_port, "<-me");
            }
            else
            {
                sprintf(output, "%d\t%s\t%s\n", i, userTable[i].name, userTable[i].ip_port);
            }
            write(sockfd, output, strlen(output));
        }
    }
}

void tell(int senderfd, int receiverID, char *msg)
{
    int senderID;
    senderID = getUserID(senderfd);
    char tellMsg[1000];
    if (userTable[receiverID].isLogin == true)
    {
        sprintf(tellMsg, "*** %s told you ***: %s\n", userTable[senderID].name, msg);
        write(userTable[receiverID].sockfd, tellMsg, strlen(tellMsg));
    }
    else
    {
        sprintf(tellMsg, "*** Error: user #%d does not exist yet. ***\n", receiverID);
        write(senderfd, tellMsg, strlen(tellMsg));
    }
}

void yell(int sockfd, char *msg)
{
    int id;
    id = getUserID(sockfd);
    char yellMsg[1000];
    sprintf(yellMsg, "*** %s yelled ***: %s\n", userTable[id].name, msg);
    broadcast(yellMsg);
}

void rename(int sockfd, char *newName)
{
    int id;
    char msg[1000];
    for (int i = 1; i <= MAX_USERS_NUMBER; i++)
    {
        if (!strcmp(userTable[i].name, newName))
        {
            sprintf(msg, "*** User '%s' already exists. ***\n", newName);
            write(sockfd, msg, strlen(msg));
            return;
        }
    }
    id = getUserID(sockfd);
    strcpy(userTable[id].name, newName);
    sprintf(msg, "*** User from %s is named '%s'. ***\n", userTable[id].ip_port, userTable[id].name);
    broadcast(msg);
}

bool isBuildInCommand(inputSegmentTable inputTable, int sockfd)
{
    if (!strcmp(inputTable.segmentation[0], "who"))
    {
        who(sockfd);
    }
    else if (!strcmp(inputTable.segmentation[0], "tell"))
    {
        char tellMsg[1000];
        memset(tellMsg, 0, sizeof(tellMsg));
        int index = 2;
        while (index < inputTable.length)
        {
            if (index == 2)
            {
                strcat(tellMsg, inputTable.segmentation[index]);
            }
            else
            {
                strcat(tellMsg, " ");
                strcat(tellMsg, inputTable.segmentation[index]);
            }
            index++;
        }
        tell(sockfd, atoi(inputTable.segmentation[1]), tellMsg);
    }
    else if (!strcmp(inputTable.segmentation[0], "yell"))
    {
        char yellMsg[1000];
        memset(yellMsg, 0, sizeof(yellMsg));
        int index = 1;
        while (index < inputTable.length)
        {
            if (index == 1)
            {
                strcat(yellMsg, inputTable.segmentation[index]);
            }
            else
            {
                strcat(yellMsg, " ");
                strcat(yellMsg, inputTable.segmentation[index]);
            }
            index++;
        }
        yell(sockfd, yellMsg);
    }
    else if (!strcmp(inputTable.segmentation[0], "name"))
    {
        rename(sockfd, inputTable.segmentation[1]);
    }
    else
    {
        return true;
    }
    return false;
}

void countDownNumPipeRoutine(pipeFD *pipeFDs, int &pipeLength)
{
    for (int i = 0; i < pipeLength; i++)
    {
        if (pipeFDs[i].isNumPipe == true)
        {
            pipeFDs[i].countDown -= 1;
        }
    }
}

void countDownOrdinaryPipeRoutine(pipeFD *pipeFDs, int &pipeLength)
{
    for (int i = 0; i < pipeLength; i++)
    {
        if (pipeFDs[i].isNumPipe == false)
        {
            pipeFDs[i].countDown -= 1;
        }
    }
}

int findInReplace(pipeFD *pipeFDs, int &pipeLength)
{
    for (int i = 0; i < pipeLength; i++)
    {
        if (pipeFDs[i].countDown == 0)
        {
            return i;
        }
    }
    return -1;
}

int findOutReplace(int pipeNum, pipeFD *pipeFDs, int &pipeLength, pipeType pipetype)
{
    for (int i = 0; i < pipeLength; i++)
    {
        if (pipeFDs[i].countDown == pipeNum)
        {
            if (pipetype == ordinary)
            {
                if (pipeFDs[i].isNumPipe == false)
                {
                    return i;
                }
            }
            else
            {
                if (pipeFDs[i].isNumPipe == true)
                {
                    return i;
                }
            }
        }
    }
    return -1;
}

int getUserPipeIndex(int from_userID, int to_userID)
{
    int index;
    for (index = 0; index < MAX_PIPES_NUMBER; index++)
    {
        if (userPipes[index].to_user == to_userID && userPipes[index].from_user == from_userID && userPipes[index].isOccupied)
        {
            return index;
        }
    }
    return -1;
}

//             從pipeTable找countdown = 0
//             如果找不到 => STDIN = 0
int createInputPipe(pipeFD *pipeFDs, command &Command, int &pipeLength, int sockfd)
{
    int to_user = getUserID(sockfd);
    if (Command.out_userpipe == true)
    {
        // if (Command.pipeoutfromuser != -1)
        // {
        //     Command.to_user = Command.from_user;
        //     Command.from_user = Command.pipeoutfromuser;
        // }
        char msg[3000];
        if (userTable[Command.from_user].isLogin == false)
        {
            sprintf(msg, "*** Error: user #%d does not exist yet. ***\n", Command.from_user);
            write(sockfd, msg, strlen(msg));
            int fd = open("/dev/null", O_RDONLY);
            return fd;
        }
        else
        {
            int userPipeIndex = getUserPipeIndex(Command.from_user, to_user);
            if (userPipeIndex == -1)
            {
                sprintf(msg, "*** Error: the pipe #%d->#%d does not exist yet. ***\n", Command.from_user, to_user);
                write(sockfd, msg, strlen(msg));
                int fd = open("/dev/null", O_RDONLY);
                return fd;
            }
            else
            {
                sprintf(msg, "*** %s (#%d) just received from %s (#%d) by '%s' ***\n", userTable[to_user].name, to_user, userTable[Command.from_user].name, Command.from_user, raw_command);
                broadcast(msg);
                close(userPipes[userPipeIndex].pipefd[1]);
                userPipes[userPipeIndex].isOccupied = false;
                Command.UPtobeClear = userPipeIndex;
                return userPipes[userPipeIndex].pipefd[0];
            }
        }
    }
    if (findInReplace(pipeFDs, pipeLength) != -1)
    {
        close(pipeFDs[findInReplace(pipeFDs, pipeLength)].FDInOut[1]);
        //代表此Command之Iutput為此pipe讀出之Output(FDInOut[0])
        return pipeFDs[findInReplace(pipeFDs, pipeLength)].FDInOut[0]; // not knowing my struct define FDInOut as pointer is right or not
    }
    else
    {
        // STDIN
        return STDIN_FILENO;
    }
}

void createPipe(pipeFD *pipeFDs, int countDown, pipeType pipetype, int &pipeLength)
{
    pipe(pipeFDs[pipeLength].FDInOut);
    pipeFDs[pipeLength].countDown = countDown;
    if (pipetype == numbered || pipetype == numberedWithError)
    {
        pipeFDs[pipeLength].isNumPipe = true;
    }
    else
    {
        ////!!!!!!!!!!!!!!!!!!!!!!!!!
        pipeFDs[pipeLength].isNumPipe = false;
    }
    pipeLength++;
}

void createUserPipe(int from_user, int to_user)
{
    userPipes[userPipesLength].from_user = from_user;
    userPipes[userPipesLength].to_user = to_user;
    pipe(userPipes[userPipesLength].pipefd);
    userPipes[userPipesLength].isOccupied = true;
    userPipesLength++;
}

//             if(filename 有值)->fopen()
//             if(pipetype = name)->STDOUT
//             if pipetype = ordinary, numbered, numberedWithError
//                        利用pipeNum找有沒有countdown = pipeNUm, 有就return
//                        沒有時需create pipe,並將其放入pipeTable
int createOutputPipe(pipeFD *pipeFDs, command &Command, int &pipeLength, int sockfd)
{
    int from_user = getUserID(sockfd);
    if (Command.filename != NULL)
    {
        int p = open(Command.filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
        return p;
    }

    if (Command.pipetype == none)
    {
        // STDOUT
        return sockfd;
    }
    else if (Command.pipetype == in_userpipe)
    {
        char msg[3000];
        // if (Command.out_userpipe == true && Command.pipeoutfromuser == -1)
        // {
        //     Command.to_user = Command.createpipetouser;
        //     Command.from_user = getUserID(sockfd);
        // }
        if (userTable[Command.to_user].isLogin == false)
        {
            sprintf(msg, "*** Error: user #%d does not exist yet. ***\n", Command.to_user);
            write(sockfd, msg, strlen(msg));
            int fd = open("/dev/null", O_WRONLY);
            return fd;
        }
        // 先判斷是否userpipeTable中已有相同之from to user
        int userPipeIndex = getUserPipeIndex(from_user, Command.to_user);
        if (userPipeIndex != -1)
        {
            sprintf(msg, "*** Error: the pipe #%d->#%d already exists. ***\n", from_user, Command.to_user);
            write(sockfd, msg, strlen(msg));
            int fd = open("/dev/null", O_WRONLY);
            return fd;
        }
        else
        {
            sprintf(msg, "*** %s (#%d) just piped '%s' to %s (#%d) ***\n", userTable[from_user].name, from_user, raw_command, userTable[Command.to_user].name, Command.to_user);
            createUserPipe(from_user, Command.to_user);
            broadcast(msg);
            return userPipes[userPipesLength - 1].pipefd[1];
        }
    }
    // else if (Command.out_userpipe == true && (!Command.pipetype == numbered) && (!Command.pipetype == ordinary))
    // {
    //     return sockfd;
    // }
    else
    {
        if (findOutReplace(Command.pipeNum, pipeFDs, pipeLength, Command.pipetype) != -1)
        {
            //代表此Command之Output為寫入此pipe之input(FDInOut[1])
            return pipeFDs[findOutReplace(Command.pipeNum, pipeFDs, pipeLength, Command.pipetype)].FDInOut[1]; // not knowing my struct define FDInOut as pointer is right or not
        }
        else
        {
            // pipeCreate
            createPipe(pipeFDs, Command.pipeNum, Command.pipetype, pipeLength);
            return pipeFDs[pipeLength - 1].FDInOut[1];
        }
    }
}

//             if(pipetype = numberedWithError)
//                        利用pipeNum找有沒有countdown = pipeNUm, 有就return
//                        沒有時需create pipe,並將其放入pipeTable
int createErrorPipe(pipeFD *pipeFDs, command Command, int &pipeLength, int ssock)
{
    if (Command.pipetype == numberedWithError)
    {
        if (findOutReplace(Command.pipeNum, pipeFDs, pipeLength, Command.pipetype) != -1)
        {
            //代表此Command之Error為寫入此pipe之input(FDInOut[1])
            return pipeFDs[findOutReplace(Command.pipeNum, pipeFDs, pipeLength, Command.pipetype)].FDInOut[1]; // not knowing my struct define FDInOut as pointer is right or not
        }
        else
        {
            // pipeCreate
            createPipe(pipeFDs, Command.pipeNum, Command.pipetype, pipeLength);
            return pipeFDs[pipeLength - 1].FDInOut[1];
        }
    }
    else
    {
        return ssock;
    }
}

void getCmdFilePath(command Command, char cmdFilePath[], EnvTable &env)
{
    if ((strcmp(Command.argv[0], "setenv")) == 0)
    {
        strcpy(cmdFilePath, "setenv");
        return;
    }
    else if ((strcmp(Command.argv[0], "printenv")) == 0)
    {
        strcpy(cmdFilePath, "printenv");
        return;
    }
    else if ((strcmp(Command.argv[0], "exit")) == 0)
    {
        strcpy(cmdFilePath, "exit");
        return;
    }
    else
    {
        ///////------
        char myenv[MAX_COMMAND_LENGTH];
        strcpy(myenv, env.val[0]);
        char delim[] = ":";
        char *pch = strtok(myenv, delim);

        while (pch != NULL)
        {
            strcpy(cmdFilePath, pch);
            if (!access(strcat(strcat(cmdFilePath, "/"), Command.argv[0]), X_OK))
            {
                return;
            }
            pch = strtok(NULL, delim);
        }

        ///////------
        // char *envPath = getenv("PATH");
        // //將環境變數作切割,分別去各個環境變數找與Command.argv[0]相同名稱的執行擋
        // char envPathTodo[MAX_COMMAND_LENGTH];
        // strcpy(envPathTodo, envPath);
        // char delim[] = ":";
        // char *pch = strtok(envPathTodo, delim);

        // while (pch != NULL) {
        //     strcpy(cmdFilePath, pch);
        //     char *fullFilePath = strcat(strcat(cmdFilePath, "/"), Command.argv[0]);
        //     FILE *fp = fopen(fullFilePath, "r");
        //     if (fp) {
        //         fclose(fp);
        //         //已透過環境變數找到其執行檔之路徑(打的開代表有),並將路徑存放在cmdFilePath
        //         return;
        //     }
        //     pch = strtok(NULL, delim);
        // }
    }
    //沒找到,設為空(後面檢測為空則errorMessage)
    strcpy(cmdFilePath, "");
}

void ChildHandler(int signo)
{
    int status;
    // https://baike.baidu.hk/item/waitpid/4071590
    while (waitpid(-1, &status, WNOHANG) > 0)
    {
    }
}

void executeCommand(command Command, char *cmdFilePath, pid_t pid_table[], int &pid_length, int inputfd, int outputfd, int errfd, EnvTable &env)
{
    // if (!strcmp(cmdFilePath, "exit")) {
    //     exit(0);
    // }
    cout << "exec1" << endl;
    if (!strcmp(cmdFilePath, "setenv"))
    {
        ///////////------
        for (int i = 0; i < env.length; ++i)
        {
            if (!strcmp(env.key[i], Command.argv[1]))
            {
                strcpy(env.val[i], Command.argv[2]);
                return;
            }
        }
        strcpy(env.key[env.length], Command.argv[1]);
        strcpy(env.val[env.length], Command.argv[2]);
        env.length++;

        // setenv(Command.argv[1], Command.argv[2], 1);
    }
    else if (!strcmp(cmdFilePath, "printenv"))
    {
        //////////----
        for (int i = 0; i < env.length; ++i)
        {
            if (!strcmp(env.key[i], Command.argv[1]))
            {
                write(outputfd, env.val[i], strlen(env.val[i]));
                write(outputfd, "\n", strlen("\n"));
                return;
            }
        }
        char *myenv = getenv(Command.argv[1]);
        if (myenv)
        {
            write(outputfd, myenv, strlen(myenv));
        }
        write(outputfd, "\n", strlen("\n"));

        // char *msg = getenv(Command.argv[1]);
        // if (msg) {
        //     cout << msg << endl;
        // }
    }
    else
    {
        cout << "exec2" << endl;
        // 1.fork() child process
        // 2.再dup(),後將設定好的in,out errPipe接上
        // 3.執行execlp(叫出cmdFilePath中的執行檔)
        signal(SIGCHLD, ChildHandler);
        pid_t c_pid = fork();

        cout << "exec3" << endl;
        // if fork error
        while (c_pid < 0)
        {
            int status;
            waitpid(-1, &status, 0);
            c_pid = fork();
        }
        cout << "exec4" << endl;
        if (c_pid == 0)
        {
            //子
            if (inputfd != STDIN_FILENO)
            {
                dup2(inputfd, STDIN_FILENO);
                close(inputfd);
            }
            if (outputfd != STDOUT_FILENO)
            {
                dup2(outputfd, STDOUT_FILENO);
            }
            if (errfd != STDERR_FILENO)
            {
                dup2(errfd, STDERR_FILENO);
            }
            if (outputfd != STDOUT_FILENO)
            {
                close(outputfd);
            }
            if (errfd != STDERR_FILENO)
            {
                close(errfd);
            }
            // dup2(inputfd, STDIN_FILENO);
            // dup2(outputfd, STDOUT_FILENO);
            // dup2(errfd, STDERR_FILENO);

            // if (inputfd != STDIN_FILENO)
            // {
            //     close(inputfd);
            // }
            // if (outputfd != STDOUT_FILENO)
            // {
            //     close(outputfd);
            // }
            // if (errfd != STDERR_FILENO)
            // {
            //     close(errfd);
            // }

            if (!strcmp(cmdFilePath, ""))
            {
                cerr << "Unknown command: [" << Command.argv[0] << "]." << endl;
            }
            else
            {
                execvp(cmdFilePath, Command.argv);
            }
            exit(0);
        }
        else
        {
            cout << "exec5" << endl;
            //父
            //等待child process完成
            //!//可以wait的時間點
            if (Command.pipetype == none || Command.filename != NULL)
            {
                cout << "exec6" << endl;
                pid_table[pid_length++] = c_pid;
                for (int i = 0; i < pid_length; ++i)
                {
                    int status;
                    waitpid(pid_table[i], &status, 0);
                }
            }
            else
            {
                pid_table[pid_length++] = c_pid;
            }

            if (Command.filename != NULL)
            {
                close(outputfd);
            }

            ////----
            // if (Command.out_userpipe)
            // {
            //     cout << "exec7" << endl;
            //     close(inputfd);
            // }
            // if (Command.pipetype == in_userpipe)
            // {
            //     cout << "exec8" << endl;
            //     close(outputfd);
            // }
            ////----
            // else
            // {
            //     pid_table[pid_length++] = c_pid;
            // }
        }
    }
}

void closePipefd(pipeFD *pipeFDs, int &pipeLength, command Command, int inputfd)
{
    for (int i = 0; i < pipeLength; i++)
    {
        if (pipeFDs[i].countDown <= 0)
        {
            close(pipeFDs[i].FDInOut[0]);
            pipeFDs[i].FDInOut[0] = -1;

            pipeLength -= 1;

            if (pipeLength > 0)
            {
                pipeFD temp = pipeFDs[pipeLength];
                pipeFDs[pipeLength] = pipeFDs[i];
                pipeFDs[i] = temp;
            }

            i -= 1;
        }
    }
    if (Command.out_userpipe == true)
    {
        if (Command.UPtobeClear != -1)
        {
            close(userPipes[Command.UPtobeClear].pipefd[0]);
            userPipe temp = userPipes[userPipesLength - 1];
            userPipes[userPipesLength - 1] = userPipes[Command.UPtobeClear];
            userPipes[Command.UPtobeClear] = temp;
            userPipesLength--;
        }
        else
        {
            close(inputfd);
        }
    }
    // for (int i = 0; i < pipeLength; i++)
    // {
    //     if (userPipes[i].isOccupied == false)
    //     {
    //         close(userPipes[i].pipefd[0]);
    //         close(userPipes[i].pipefd[1]);
    //         userPipesLength--;

    //         if (pipeLength > 0)
    //         {
    //             userPipe temp = userPipes[userPipesLength];
    //             userPipes[userPipesLength] = userPipes[i];
    //             userPipes[userPipesLength] = temp;
    //         }

    //         i -= 1;
    //     }
    // }
}

//建socket bind listen
int passiveTCP(uint16_t ser_port)
{
    int sockfd, optval = 1;
    sockaddr_in serv_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        perror("Error: socket failed");
        exit(0);
    }
    bzero((char *)&serv_addr, sizeof(serv_addr));

    // memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(ser_port);

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1)
    {
        perror("Error: setsockopt failed");
        exit(0);
    }

    if (bind(sockfd, (sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("Error: bind failed");
        exit(0);
    }

    if (listen(sockfd, 5) == -1)
    {
        perror("Error: listen failed");
        exit(0);
    }

    return sockfd;
}

int main(int argc, char *argv[])
{

    // initPath, setenv()
    ////////-------------
    // setenv("PATH", "bin:.", 1);
    userTableInit(userTable);
    userPipesLength = 0;
    char input[MAX_SINGLE_INPUT_LENGTH];
    memset(&input, '\0', sizeof(input));

    // pid_t pid_table[MAX_COMMAND_NUMBER]; ////!////
    // int pid_length = 0;
    //  char cmdFilePath[MAX_COMMAND_LENGTH];
    // bool lastCommandIsNum = false;
    inputSegmentTable inputTable;

    ////
    fd_set rfds;
    fd_set afds;
    int nfds;
    int msock;
    int port = atoi(argv[1]);
    msock = passiveTCP(port);

    nfds = getdtablesize();
    FD_ZERO(&afds);
    FD_SET(msock, &afds);

    for (int i = 1; i <= MAX_USERS_NUMBER; ++i)
    {
        userTable[i].isLogin = false;
    }

    while (true)
    {
        // 1.check ON/OFF
        memcpy(&rfds, &afds, sizeof(rfds));
        if (select(nfds, &rfds, (fd_set *)0, (fd_set *)0, (struct timeval *)0) < 0)
        {
            perror("Error:select error");
            continue;
        }
        // if (select(nfds, &rfds, NULL, NULL, NULL) < 0)
        // {
        //     perror("Error:select error");
        //     continue;
        // }
        // 2.接單accept
        if (FD_ISSET(msock, &rfds))
        {
            sockaddr_in cl_addr;
            int cl_addr_len;
            int ssock;
            cl_addr_len = sizeof(cl_addr);
            ssock = accept(msock, (sockaddr *)&cl_addr, (socklen_t *)&cl_addr_len);
            if (ssock < 0)
            {
                perror("Error: accept failed");
                exit(0);
            }
            FD_SET(ssock, &afds);
            userLogin(ssock, cl_addr);

            write(ssock, "% ", strlen("% "));
        }

        // R/W(slave socket)
        for (int sockfd = 0; sockfd < nfds; ++sockfd)
        {
            if (msock != sockfd && FD_ISSET(sockfd, &rfds))
            {
                //執行socketshell or execute command
                char input[MAX_SINGLE_INPUT_LENGTH];
                memset(&input, '\0', sizeof(input));
                // pipeFD pipeFDs[MAX_PIPES_NUMBER];
                // int pipeLength = 0;
                pid_t pid_table[MAX_COMMAND_NUMBER]; ////!////
                int pid_length = 0;
                char cmdFilePath[MAX_COMMAND_LENGTH];
                bool lastCommandIsNum = false;
                inputSegmentTable inputTable;
                int userIndex = getUserID(sockfd);
                // cout << "% ";
                // down to the end

                char readBuffer[MAX_SINGLE_INPUT_LENGTH];
                memset(&input, '\0', sizeof(input));
                initInputTable(inputTable);

                // if(!cin.getline(input,sizeof(input))){
                //     break;
                // }

                ///////////////////--------
                memset(&input, '\0', sizeof(input));
                do
                {
                    memset(&readBuffer, '\0', sizeof(readBuffer));
                    read(sockfd, readBuffer, sizeof(readBuffer));
                    strcat(input, readBuffer);
                    //// sizeof -> strlen
                } while (readBuffer[strlen(readBuffer) - 1] != '\n');

                strtok(input, "\r\n");
                strcpy(raw_command, input);

                segmentInput(input, inputTable);
                if (inputTable.length == 0)
                {
                    continue;
                }

                if (!strcmp(inputTable.segmentation[0], "exit"))
                {
                    userLogout(afds, sockfd);
                    continue;
                }
                if (isBuildInCommand(inputTable, sockfd))
                {
                    /////////////////////!!!!!!!!!!!!!!!!
                    // Handle numbered pipe for new line input command.
                    countDownNumPipeRoutine(userTable[userIndex].pipeFDs, userTable[userIndex].pipeLength);
                    lastCommandIsNum = false;
                    /////////////////////!!!!!!!!!!!!!!!!

                    // inputTable.length:一行指令的command個數
                    while (inputTable.index < inputTable.length)
                    {
                        command Command;
                        int inputfd, outputfd, errfd;

                        // Init cmdFilePath
                        memset(&cmdFilePath, '\0', sizeof(cmdFilePath));

                        initCommand(Command);
                        parseCommand(inputTable, Command, sockfd);

                        cout << "parse command" << endl;
                        cout << "raw_command: " << raw_command << endl;
                        cout << "out_user_pipe:" << Command.out_userpipe << endl;
                        cout << "pipetype: " << Command.pipetype << endl;
                        cout << "from_user: " << Command.from_user << endl;
                        cout << "to_user: " << Command.to_user << endl;
                        cout << "parse command end" << endl;

                        /////////////////////!!!!!!!!!!!!!!!!
                        // Handle numbered pipe in middle.
                        if (lastCommandIsNum)
                        {
                            countDownNumPipeRoutine(userTable[userIndex].pipeFDs, userTable[userIndex].pipeLength);
                            lastCommandIsNum = false;
                        }
                        else
                        {
                            // TODO: Think the logic.
                            countDownOrdinaryPipeRoutine(userTable[userIndex].pipeFDs, userTable[userIndex].pipeLength);
                        }

                        if (Command.pipetype == numbered || Command.pipetype == numberedWithError)
                        {
                            lastCommandIsNum = true;
                        }
                        /////////////////////!!!!!!!!!!!!!!!!
                        cout << "\n\n"
                             << endl;
                        cout << "userID: " << getUserID(sockfd) << endl;
                        cout << "Hello1" << endl;
                        // FDTable index
                        inputfd = createInputPipe(userTable[userIndex].pipeFDs, Command, userTable[userIndex].pipeLength, sockfd);
                        cout << "Hello2" << endl;
                        outputfd = createOutputPipe(userTable[userIndex].pipeFDs, Command, userTable[userIndex].pipeLength, sockfd);
                        cout << "Hello3" << endl;
                        errfd = createErrorPipe(userTable[userIndex].pipeFDs, Command, userTable[userIndex].pipeLength, sockfd);

                        cout << "infd: " << inputfd << endl;
                        cout << "outfd: " << outputfd << endl;
                        cout << "errorfd: " << errfd << endl;
                        cout << "Hello end" << endl;

                        getCmdFilePath(Command, cmdFilePath, userTable[userIndex].envTable);

                        cout << "getPath end" << endl;

                        ///!///
                        executeCommand(Command, cmdFilePath, pid_table, pid_length, inputfd, outputfd, errfd, userTable[userIndex].envTable);

                        cout << "execute command end" << endl;

                        closePipefd(userTable[userIndex].pipeFDs, userTable[userIndex].pipeLength, Command, inputfd);

                        cout << "close pipe end" << endl;
                    }
                }
                write(sockfd, "% ", strlen("% "));
            }
        }
        // close(ssock);
    }
}