#include <iostream>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/wait.h>
//新增
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/shm.h>
#include <arpa/inet.h>
#include <sys/stat.h>

using namespace std;

#define MAX_SINGLE_INPUT_LENGTH 15000
#define MAX_COMMAND_LENGTH 256
#define MAX_PIPES_NUMBER 1000
#define MAX_COMMAND_NUMBER 5000
#define ENV_NUMBER 100
#define ENV_LENGTH 500
#define NAME_LENGTH 500
#define MAX_IP_PORT_LENGTH 30
#define MAX_PATH_SIZE 30
#define MAX_USER 30
#define MAX_MESSAGE_LENGTH 1000

#define SHM_USER_INFO_KEY 1234
#define SHM_MESSAGE_KEY 2345
#define SHM_FIFO_INFO_KEY 5678
#define FIFO_PATH "user_pipe/"

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
    // userPipeInput,
    userPipeOutput,
};

struct command
{
    int argc;
    char *argv[MAX_COMMAND_LENGTH];
    pipeType pipetype = none;
    int pipeNum = 0; //需要預設嗎
    char *filename = NULL;
    int from_user = -1;
    int to_user = -1;
    bool userPipeInput = false;
};

struct pipeFD
{
    int FDInOut[2];
    int countDown = -1;
    bool isNumPipe = false;
};

struct UserInfo
{
    bool is_login = false;
    int client_id;
    int pid;
    char name[NAME_LENGTH];
    char ip_port[MAX_IP_PORT_LENGTH];
    // int address;
};

struct FIFO
{
    char name[MAX_PATH_SIZE];
    int in_fd;
    int out_fd;
    bool is_used;
};

struct FIFOInfo
{
    FIFO fifo[MAX_USER][MAX_USER];
};

// Global variable.
int shmid_user_info;
int shmid_message;
int shmid_fifo_info;
int g_client_id;

//建socket bind listen
int passiveTCP(uint16_t ser_port)
{
    int sockfd;
    int optval = 1;
    sockaddr_in sin;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("Error: socket failed");
        exit(0);
    }

    bzero((char *)&sin, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(ser_port);

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1)
    {
        perror("Error: setsockopt failed");
        exit(0);
    }

    if (bind(sockfd, (struct sockaddr *)&sin, sizeof(sin)) < 0)
    {
        perror("Error: bind failed");
        exit(0);
    }

    if (listen(sockfd, 5) < 0)
    {
        perror("Error: listen failed");
        exit(0);
    }
    return sockfd;
}

void DeleteSHM()
{
    // Delete all shared memory.
    shmctl(shmid_user_info, IPC_RMID, NULL);
    shmctl(shmid_message, IPC_RMID, NULL);
    shmctl(shmid_fifo_info, IPC_RMID, NULL);
}

void ServerSigHandler(int sig)
{
    // 避免zombie process.
    if (sig == SIGCHLD)
    {
        while (waitpid(-1, NULL, WNOHANG) > 0)
            ;
    }
    else if (sig == SIGINT || sig == SIGQUIT || sig == SIGTERM)
    {
        DeleteSHM();
        exit(0);
    }
    signal(sig, ServerSigHandler);
}

void InitUserInfoSHM()
{
    UserInfo *shm = (UserInfo *)shmat(shmid_user_info, NULL, 0);

    if (shm < (UserInfo *)0)
    {
        perror("Error: shmat() failed");
        exit(1);
    }
    for (int i = 0; i < MAX_USER; ++i)
    {
        shm[i].is_login = false;
    }
    shmdt(shm);
}

FIFOInfo *GetFIFOInfoSHM()
{
    FIFOInfo *shm_fifo = (FIFOInfo *)shmat(shmid_fifo_info, NULL, 0);

    if (shm_fifo < (FIFOInfo *)0)
    {
        perror("Error: get_new_id() failed");
        exit(1);
    }

    return shm_fifo;
}

void InitFIFOInfoSHM()
{
    FIFOInfo *shm_fifo = GetFIFOInfoSHM();

    for (int i = 0; i < MAX_USER; ++i)
    {
        for (int j = 0; j < MAX_USER; ++j)
        {
            char name[MAX_PATH_SIZE];
            memset(&shm_fifo->fifo[i][j].name, 0, sizeof(name));
            shm_fifo->fifo[i][j].in_fd = -1;
            shm_fifo->fifo[i][j].out_fd = -1;
            shm_fifo->fifo[i][j].is_used = 0;
        }
    }
    shmdt(shm_fifo);
}

void InitSHM()
{
    int shm_user_info_size = sizeof(UserInfo) * MAX_USER;
    int message_size = sizeof(char) * MAX_MESSAGE_LENGTH;
    int fifo_info_size = sizeof(FIFOInfo);

    shmid_user_info = shmget(SHM_USER_INFO_KEY, shm_user_info_size, 0666 | IPC_CREAT);
    if (shmid_user_info < 0)
    {
        perror("Error: init_shm() failed");
        exit(1);
    }
    shmid_message = shmget(SHM_MESSAGE_KEY, message_size, 0666 | IPC_CREAT);
    if (shmid_message < 0)
    {
        perror("Error: init_shm() failed");
        exit(1);
    }
    shmid_fifo_info = shmget(SHM_FIFO_INFO_KEY, fifo_info_size, 0666 | IPC_CREAT);
    if (shmid_fifo_info < 0)
    {
        perror("Error: init_shm() failed");
        exit(1);
    }

    InitUserInfoSHM();
    InitFIFOInfoSHM();
}

UserInfo *GetUserSHM()
{
    UserInfo *shm = (UserInfo *)shmat(shmid_user_info, NULL, 0);

    if (shm < (UserInfo *)0)
    {
        cerr << "Error: get_new_id() failed" << endl;
        exit(1);
    }
    return shm;
}

// 返回第一個available client ID.
int GetClientIDFromSHM()
{
    UserInfo *shm = GetUserSHM();

    for (int i = 0; i < MAX_USER; ++i)
    {
        if (!shm[i].is_login)
        {
            shm[i].is_login = true;
            shmdt(shm);
            return (i + 1);
        }
    }
    shmdt(shm);
    return -1;
}

void AddNewUser(int client_id, sockaddr_in client_address)
{
    UserInfo *shm = GetUserSHM();
    int shm_index = client_id - 1;

    shm[shm_index].is_login = true;
    shm[shm_index].client_id = client_id;
    shm[shm_index].pid = getpid();
    strcpy(shm[shm_index].name, "(no name)");
    char port[10];
    sprintf(port, "%d", ntohs(client_address.sin_port));
    strcpy(shm[shm_index].ip_port, inet_ntoa(client_address.sin_addr));
    strcat(shm[shm_index].ip_port, ":");
    strcat(shm[shm_index].ip_port, port);
}

void SigHandler(int sig)
{
    // 接收到其他user的message
    if (sig == SIGUSR1)
    {
        char *msg = (char *)shmat(shmid_message, NULL, 0);

        if (msg < (char *)0)
        {
            perror("Error: shmat() failed");
            exit(1);
        }
        if (write(STDOUT_FILENO, msg, strlen(msg)) < 0)
        {
            perror("Error: broadcast_catch() failed");
        }
        shmdt(msg);
    }
    // 其他user透過FIFO傳送user pipe input
    else if (sig == SIGUSR2)
    {
        FIFOInfo *shm_fifo = GetFIFOInfoSHM();
        int i;
        for (i = 0; i < MAX_USER; ++i)
        {
            if (shm_fifo->fifo[i][g_client_id - 1].out_fd == -1 && shm_fifo->fifo[i][g_client_id - 1].name[0] != 0)
            {
                // get readfd
                shm_fifo->fifo[i][g_client_id - 1].out_fd = open(shm_fifo->fifo[i][g_client_id - 1].name, O_RDONLY);

                // cout << "jjjj" << endl;
                // cout << shm_fifo->fifo[i][g_client_id - 1].in_fd << endl;
                // cout << shm_fifo->fifo[i][g_client_id - 1].out_fd << endl;
            }
        }
        shmdt(shm_fifo);
    }
    // Terminate the process
    else if (sig == SIGINT || sig == SIGQUIT || sig == SIGTERM)
    {
        // LogoutUser(g_client_id);
    }
    signal(sig, SigHandler);
}

void WriteWelcomeMsg()
{
    char message[] = "****************************************\n** Welcome to the information server. **\n****************************************\n";
    write(STDOUT_FILENO, message, strlen(message));
}

char *GetMessageSHM()
{
    char *shm = (char *)shmat(shmid_message, NULL, 0);
    if (shm < (char *)0)
    {
        perror("Error: get_new_id() failed");
        exit(1);
    }
    return shm;
}

UserInfo *GetUserByID(int id)
{
    UserInfo *shm = GetUserSHM();
    for (int i = 0; i < MAX_USER; ++i)
    {
        if ((shm[i].client_id == id) && (shm[i].is_login))
        {
            UserInfo *res = &shm[i];
            return res;
        }
    }
    return NULL;
}

void BroadcastMsg(const char *msg)
{
    char *shm_msg = GetMessageSHM();
    sprintf(shm_msg, "%s", msg);
    shmdt(shm_msg);

    // TODO
    usleep(500);

    UserInfo *shm_user = GetUserSHM();
    for (int i = 0; i < MAX_USER; ++i)
    {
        if (shm_user[i].is_login)
        {
            kill(shm_user[i].pid, SIGUSR1);
        }
    }
    shmdt(shm_user);
}

void LoginBroadcast(int client_id)
{
    UserInfo *current_user = GetUserByID(client_id);

    char msg[2000];
    sprintf(msg, "*** User '%s' entered from %s. ***\n", current_user->name, current_user->ip_port);
    BroadcastMsg(msg);
}

void LoginUser(int client_id)
{
    // WriteWelcomeMsg
    WriteWelcomeMsg();

    // LoginBroadcast
    LoginBroadcast(client_id);
}

void initInputTable(inputSegmentTable &inputTable)
{
    for (int i = 0; i < inputTable.length; i++)
    {
        memset(inputTable.segmentation[i], '\0', sizeof(inputTable.segmentation[i]));
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

void Yell(const char *msg)
{
    UserInfo *current_user = GetUserByID(g_client_id);

    char output[1000];
    sprintf(output, "*** %s yelled ***: %s\n", current_user->name, msg);
    BroadcastMsg(output);
}

void Name(const char *name)
{
    UserInfo *shm_user = GetUserSHM();
    char output[1000];

    for (size_t i = 0; i < MAX_USER; ++i)
    {
        if (!strcmp(shm_user[i].name, name))
        {
            UserInfo *current_user = GetUserByID(g_client_id);

            sprintf(output, "*** User '%s' already exists. ***\n", name);
            write(STDOUT_FILENO, output, strlen(output));
            return;
        }
    }
    strcpy(shm_user[g_client_id - 1].name, name);
    shmdt(shm_user);

    UserInfo *current_user = GetUserByID(g_client_id);

    sprintf(output, "*** User from %s is named '%s'. ***\n", current_user->ip_port, current_user->name);
    BroadcastMsg(output);
}

void Who()
{
    int temp_id = 0;
    UserInfo *shm_cli = GetUserSHM();
    UserInfo *cur_cli = GetUserByID(g_client_id);

    cout << "<ID>\t"
         << "<nickname>\t"
         << "<IP:port>\t"
         << "<indicate me>" << endl;

    for (size_t id = 0; id < MAX_USER; ++id)
    {
        if (GetUserByID(id + 1) != NULL)
        {
            temp_id = id + 1;
            UserInfo *temp = GetUserByID(temp_id);
            cout << temp->client_id << "\t" << temp->name << "\t" << temp->ip_port;

            if (temp_id == cur_cli->client_id)
            {
                cout << "\t"
                     << "<-me" << endl;
            }
            else
            {
                cout << "\t" << endl;
            }
        }
    }
    shmdt(shm_cli);
}

void BroadcastToOne(char *msg, int cur_id, int target_id)
{
    UserInfo *shm = GetUserSHM();
    UserInfo *cur_cli = GetUserByID(cur_id);

    char *shm_msg = GetMessageSHM();
    sprintf(shm_msg, "%s", msg);
    shmdt(shm_msg);
    usleep(500);

    shm = GetUserSHM();
    kill(shm[target_id - 1].pid, SIGUSR1);
    shmdt(shm);
}

void Tell(int target_id, char *msg)
{
    UserInfo *shm_cli = GetUserSHM();
    UserInfo *cur_cli = GetUserByID(g_client_id);
    char output[1000];

    if (shm_cli[target_id - 1].is_login)
    {
        sprintf(output, "*** %s told you ***: %s\n", cur_cli->name, msg);
        BroadcastToOne(output, g_client_id, target_id);
    }
    else
    {
        cerr << "*** Error: user #" << to_string(target_id) << " does not exist yet. ***" << endl;
    }
    shmdt(shm_cli);
}

bool IsBuildInCommand(inputSegmentTable input_table)
{
    if (!strcmp(input_table.segmentation[0], "yell"))
    {
        char msg[1000] = "";
        for (int i = 1; i < input_table.length; i++)
        {
            strcat(msg, input_table.segmentation[i]);
            strcat(msg, " ");
        }
        Yell(msg);
    }
    else if (!strcmp(input_table.segmentation[0], "name"))
    {
        Name(input_table.segmentation[1]);
    }
    else if (!strcmp(input_table.segmentation[0], "who"))
    {
        Who();
    }
    else if (!strcmp(input_table.segmentation[0], "tell"))
    {
        char msg[1000] = "";

        for (int i = 2; i < input_table.length; i++)
        {
            strcat(msg, input_table.segmentation[i]);
            strcat(msg, " ");
        }

        Tell(atoi(input_table.segmentation[1]), msg);
    }
    else if (!strcmp(input_table.segmentation[0], "printenv"))
    {
        char *msg = getenv(input_table.segmentation[1]);
        if (msg)
        {
            cout << msg << endl;
        }
    }
    else if (!strcmp(input_table.segmentation[0], "setenv"))
    {
        setenv(input_table.segmentation[1], input_table.segmentation[2], 1);
    }
    else
    {
        return false;
    }

    return true;
}

void initCommand(command &Command)
{
    Command.argc = 0;
    memset(Command.argv, '\0', sizeof(Command.argv));
    Command.pipetype = none;
    Command.pipeNum = 0; //需要預設嗎
    Command.filename = NULL;
    Command.from_user = -1;
    Command.to_user = -1;
    Command.userPipeInput = false;
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
void parseCommand(inputSegmentTable &inputTable, command &Command)
{
    int currentIndex = 0;

    while (inputTable.index < inputTable.length)
    {
        if (inputTable.segmentation[inputTable.index][0] != '>' && inputTable.segmentation[inputTable.index][0] != '<' && inputTable.segmentation[inputTable.index][0] != '|')
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
        else if (inputTable.segmentation[inputTable.index][0] == '>')
        {
            if (inputTable.segmentation[inputTable.index][1] != '\0')
            {
                Command.to_user = atoi(&inputTable.segmentation[inputTable.index][1]);
                Command.pipetype = userPipeOutput;
                inputTable.index++;

                if (inputTable.length > inputTable.index && inputTable.segmentation[inputTable.index][0] == '<')
                {
                    // inputTable.index++;
                    continue;
                }
                break;
            }
            Command.filename = inputTable.segmentation[++inputTable.index];
            inputTable.index++;
            break;
        }
        else if (inputTable.segmentation[inputTable.index][0] == '<')
        {
            Command.from_user = atoi(&inputTable.segmentation[inputTable.index][1]);
            Command.userPipeInput = true;
            inputTable.index++;
            continue;
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

int GetUserPipeFd(int source_id, int cur_id)
{
    UserInfo *shm_cli = GetUserSHM();
    FIFOInfo *shm_fifo = GetFIFOInfoSHM();

    shm_fifo->fifo[source_id - 1][cur_id - 1].in_fd = -1;
    int in_fd = shm_fifo->fifo[source_id - 1][cur_id - 1].out_fd;
    shm_fifo->fifo[source_id - 1][cur_id - 1].is_used = true;

    shmdt(shm_cli);
    shmdt(shm_fifo);

    return in_fd;
}

//             從pipeTable找countdown = 0
//             如果找不到 => STDIN = 0
int createInputPipe(pipeFD *pipeFDs, command Command, int &pipeLength, char *raw_command)
{
    if (Command.userPipeInput)
    {
        // target id does not exist
        if (GetUserByID(Command.from_user) == NULL)
        {
            cout << "*** Error: user #" << Command.from_user << " does not exist yet. ***" << endl;
            int fd = open("/dev/null", O_RDONLY);
            return fd;
        }
        else
        {
            // target id exists
            // Handle user pipe.
            // check if user pipe already exists
            FIFOInfo *shm_fifo = GetFIFOInfoSHM();

            if (shm_fifo->fifo[Command.from_user - 1][g_client_id - 1].out_fd == -1)
            {
                // cannot find any userpipe's target id is current client id
                cout << "*** Error: the pipe #" << Command.from_user << "->#" << g_client_id;
                cout << " does not exist yet. ***" << endl;
                int fd = open("/dev/null", O_RDONLY);
                return fd;
            }
            shmdt(shm_fifo);

            char msg[3000];
            UserInfo *source_cli = GetUserByID(Command.from_user);
            UserInfo *current_user = GetUserByID(g_client_id);
            sprintf(msg, "*** %s (#%d) just received from %s (#%d) by '%s' ***\n", current_user->name, current_user->client_id, source_cli->name, Command.from_user, raw_command);
            BroadcastMsg(msg);

            usleep(500);

            // get user input pipe fd.
            return GetUserPipeFd(Command.from_user, g_client_id);
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

void CreateUserPipe(int client_id, int target_id)
{
    char fifopath[MAX_PATH_SIZE];

    sprintf(fifopath, "%suser_pipe_%d_%d", FIFO_PATH, client_id, target_id);
    if (mkfifo(fifopath, 0666 | S_IFIFO < 0))
    {
        cerr << "mkfifo error" << endl;
        exit(0);
    }

    UserInfo *target_cli = GetUserByID(target_id);
    FIFOInfo *shm_fifo = GetFIFOInfoSHM();
    strncpy(shm_fifo->fifo[g_client_id - 1][target_id - 1].name, fifopath, MAX_PATH_SIZE);

    // signal target client to open fifo and read
    kill(target_cli->pid, SIGUSR2);
    shmdt(shm_fifo);
}

int GetUserPipeOut(int target_id)
{
    char fifopath[MAX_PATH_SIZE];
    sprintf(fifopath, "%suser_pipe_%d_%d", FIFO_PATH, g_client_id, target_id);

    UserInfo *shm_cli = GetUserSHM();
    FIFOInfo *shm_fifo = GetFIFOInfoSHM();

    int out_fd = open(fifopath, O_WRONLY);
    shm_fifo->fifo[g_client_id - 1][target_id - 1].in_fd = out_fd;

    // cout << "Hello" << endl;
    // cout << shm_fifo->fifo[g_client_id - 1][target_id - 1].in_fd << endl;
    // cout << shm_fifo->fifo[g_client_id - 1][target_id - 1].out_fd << endl;

    shmdt(shm_cli);
    shmdt(shm_fifo);

    return out_fd;
}

//             if(filename 有值)->fopen()
//             if(pipetype = name)->STDOUT
//             if pipetype = ordinary, numbered, numberedWithError
//                        利用pipeNum找有沒有countdown = pipeNUm, 有就return
//                        沒有時需create pipe,並將其放入pipeTable
int createOutputPipe(pipeFD *pipeFDs, command Command, int &pipeLength, char *raw_command)
{
    if (Command.filename != NULL)
    {
        int p = open(Command.filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
        return p;
    }

    if (Command.pipetype == userPipeOutput)
    {

        if (GetUserByID(Command.to_user) == NULL)
        {
            cout << "*** Error: user #" << Command.to_user << " does not exist yet. ***" << endl;
            int fd = open("/dev/null", O_RDONLY);
            return fd;
        }
        else
        {
            // Create FIFO
            // check if user pipe already exists
            FIFOInfo *shm_fifo = GetFIFOInfoSHM();

            if (shm_fifo->fifo[g_client_id - 1][Command.to_user - 1].in_fd != -1)
            {
                cout << "*** Error: the pipe #" << g_client_id << "->#" << Command.to_user;
                cout << " already exists. ***" << endl;
                int fd = open("/dev/null", O_RDONLY);
                return fd;
            }
            shmdt(shm_fifo);

            char msg[3000];
            UserInfo *target_cli = GetUserByID(Command.to_user);
            UserInfo *current_user = GetUserByID(g_client_id);
            sprintf(msg, "*** %s (#%d) just piped '%s' to %s (#%d) ***\n", current_user->name, current_user->client_id, raw_command, target_cli->name, target_cli->client_id);
            BroadcastMsg(msg);

            // Create user pipe
            CreateUserPipe(g_client_id, Command.to_user);

            return GetUserPipeOut(Command.to_user);
        }
    }
    else if (Command.pipetype == none || (Command.userPipeInput && (Command.pipetype != numbered && Command.pipetype != ordinary)))
    {
        // STDOUT
        return STDOUT_FILENO;
    }
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
int createErrorPipe(pipeFD *pipeFDs, command Command, int &pipeLength)
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
    else if (Command.pipetype == userPipeOutput)
    {
        // Create FIFO
        // check if user pipe already exists
        FIFOInfo *shm_fifo = GetFIFOInfoSHM();

        if (shm_fifo->fifo[g_client_id - 1][Command.to_user - 1].in_fd != -1)
        {
            int fd = open("/dev/null", O_RDONLY);
            return fd;
        }
        shmdt(shm_fifo);

        if (GetUserByID(Command.to_user) == NULL)
        {
            int fd = open("/dev/null", O_RDONLY);
            return fd;
        }
    }
    else
    {
        return STDERR_FILENO;
    }
}

void getCmdFilePath(command Command, char cmdFilePath[])
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
        char *envPath = getenv("PATH");
        //將環境變數作切割,分別去各個環境變數找與Command.argv[0]相同名稱的執行擋
        char envPathTodo[MAX_COMMAND_LENGTH];
        strcpy(envPathTodo, envPath);
        char delim[] = ":";
        char *pch = strtok(envPathTodo, delim);

        while (pch != NULL)
        {
            strcpy(cmdFilePath, pch);
            char *fullFilePath = strcat(strcat(cmdFilePath, "/"), Command.argv[0]);
            FILE *fp = fopen(fullFilePath, "r");
            if (fp)
            {
                fclose(fp);
                //已透過環境變數找到其執行檔之路徑(打的開代表有),並將路徑存放在cmdFilePath
                return;
            }
            pch = strtok(NULL, delim);
        }
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

void EraseUserPipe(int id)
{
    FIFOInfo *shm_fifo = GetFIFOInfoSHM();

    if (shm_fifo->fifo[id - 1][g_client_id - 1].is_used)
    {
        shm_fifo->fifo[id - 1][g_client_id - 1].in_fd = -1;
        shm_fifo->fifo[id - 1][g_client_id - 1].out_fd = -1;
        shm_fifo->fifo[id - 1][g_client_id - 1].is_used = false;

        unlink(shm_fifo->fifo[id - 1][g_client_id - 1].name);
        memset(&shm_fifo->fifo[id - 1][g_client_id - 1].name, 0, sizeof(shm_fifo->fifo[id - 1][g_client_id - 1].name));
    }
}

void executeCommand(command Command, char *cmdFilePath, pid_t pid_table[], int &pid_length, int inputfd, int outputfd, int errfd)
{
    // 1.fork() child process
    // 2.再dup(),後將設定好的in,out errPipe接上
    // 3.執行execlp(叫出cmdFilePath中的執行檔)
    signal(SIGCHLD, ChildHandler);
    pid_t c_pid = fork();

    // if fork error
    while (c_pid < 0)
    {
        int status;
        waitpid(-1, &status, 0);
        c_pid = fork();
    }

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
        //父
        //等待child process完成
        //!//可以wait的時間點
        if (Command.pipetype == none || Command.pipetype == userPipeOutput || Command.userPipeInput || Command.filename != NULL)
        {
            pid_table[pid_length++] = c_pid;
            for (int i = 0; i < pid_length; ++i)
            {
                int status;
                waitpid(pid_table[i], &status, 0);
            }
            if (Command.filename != NULL)
            {
                close(outputfd);
            }
            if (Command.pipetype == userPipeOutput)
            {
                close(outputfd);
            }
            if (Command.userPipeInput)
            {
                close(inputfd);
            }
            if (Command.from_user != -1)
            {
                EraseUserPipe(Command.from_user);
            }
        }
        else
        {
            pid_table[pid_length++] = c_pid;
        }
    }
}

void closePipefd(pipeFD *pipeFDs, int &pipeLength)
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
}

void socketShell(int client_id)
{
    char input[MAX_SINGLE_INPUT_LENGTH];
    char raw_command[MAX_SINGLE_INPUT_LENGTH];
    inputSegmentTable inputTable;
    pipeFD pipeFDs[MAX_PIPES_NUMBER];
    int pipeLength = 0;
    bool lastCommandIsNum = false;
    char cmdFilePath[MAX_COMMAND_LENGTH];
    pid_t pid_table[MAX_COMMAND_NUMBER]; ////!////
    int pid_length = 0;

    // cout << client_id << endl;
    // cout << "Hello" << endl;
    g_client_id = client_id;

    while (true)
    {
        cout << "% ";

        memset(&input, '\0', sizeof(input));
        memset(&raw_command, '\0', sizeof(raw_command));
        initInputTable(inputTable);

        if (!cin.getline(input, sizeof(input)))
        {
            break;
        }

        strtok(input, "\r\n");
        strcpy(raw_command, input);

        if (!strcmp(input, "\0"))
        {
            continue;
        }
        else if (!strcmp(input, "exit"))
        {
            return;
        }

        // segment input
        segmentInput(input, inputTable);

        // countdown numbered pipe routine
        countDownNumPipeRoutine(pipeFDs, pipeLength);
        lastCommandIsNum = false;

        if (!IsBuildInCommand(inputTable))
        {

            // inputTable.index < inputTable.length
            while (inputTable.index < inputTable.length)
            {
                command Command;
                int inputfd, outputfd, errfd;

                // Init cmdFilePath
                memset(&cmdFilePath, '\0', sizeof(cmdFilePath));

                initCommand(Command);
                parseCommand(inputTable, Command);

                // Handle numbered pipe in middle.
                if (lastCommandIsNum)
                {
                    countDownNumPipeRoutine(pipeFDs, pipeLength);
                    lastCommandIsNum = false;
                }
                else
                {
                    countDownOrdinaryPipeRoutine(pipeFDs, pipeLength);
                }

                if (Command.pipetype == numbered || Command.pipetype == numberedWithError)
                {
                    lastCommandIsNum = true;
                }

                // cout << "Hello0" << endl;
                inputfd = createInputPipe(pipeFDs, Command, pipeLength, raw_command);
                // cout << "Hello1" << endl;
                outputfd = createOutputPipe(pipeFDs, Command, pipeLength, raw_command);
                // cout << "Hello2" << endl;
                errfd = createErrorPipe(pipeFDs, Command, pipeLength);
                // cout << "Hello3" << endl;

                // cout << inputfd << endl;
                // cout << outputfd << endl;
                // cout << errfd << endl;

                // getCmdFilePath
                getCmdFilePath(Command, cmdFilePath);

                // executeCommand
                executeCommand(Command, cmdFilePath, pid_table, pid_length, inputfd, outputfd, errfd);

                // closePipefd
                closePipefd(pipeFDs, pipeLength);
            }
        }
    }
}

void LogoutBroadcast(int client_id)
{
    UserInfo *current_user = GetUserByID(client_id);

    char msg[2000];
    sprintf(msg, "*** User '%s' left. ***\n", current_user->name);
    BroadcastMsg(msg);
}

void cleanClientInfo(int client_id)
{
    UserInfo *shm_user_info = GetUserSHM();
    shm_user_info[client_id - 1].is_login = false;
    shmdt(shm_user_info);
}

void cleanFIFOInfo(int client_id)
{
    FIFOInfo *shm_fifo = GetFIFOInfoSHM();

    for (size_t i = 0; i < MAX_USER; i++)
    {
        if (shm_fifo->fifo[i][client_id - 1].out_fd != -1)
        {
            // read out message in the unused fifo
            char buf[1024];
            while (read(shm_fifo->fifo[i][client_id - 1].out_fd, &buf, sizeof(buf)) > 0)
            {
            }
            shm_fifo->fifo[i][client_id - 1].out_fd = -1;
            shm_fifo->fifo[i][client_id - 1].in_fd = -1;
            shm_fifo->fifo[i][client_id - 1].is_used = false;
            unlink(shm_fifo->fifo[i][client_id - 1].name);
            memset(shm_fifo->fifo[i][client_id - 1].name, 0, sizeof(shm_fifo->fifo[client_id - 1][i].name));
        }
    }
    shmdt(shm_fifo);
}

void LogoutUser(int client_id)
{
    // Broadcast
    LogoutBroadcast(client_id);

    // clean client info
    cleanClientInfo(client_id);

    // clean FIFO info
    cleanFIFOInfo(client_id);

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    exit(0);
}

int main(int argc, char *argv[])
{
    setenv("PATH", "bin:.", 1);

    int msock, ssock, child_pid;
    socklen_t cl_addr_len;
    struct sockaddr_in cl_addr;

    int port = atoi(argv[1]);
    msock = passiveTCP(port);

    signal(SIGCHLD, ServerSigHandler);
    signal(SIGINT, ServerSigHandler);
    signal(SIGQUIT, ServerSigHandler);
    signal(SIGTERM, ServerSigHandler);

    InitSHM();

    while (true)
    {
        cl_addr_len = sizeof(cl_addr);
        ssock = accept(msock, (struct sockaddr *)&cl_addr, (socklen_t *)&cl_addr_len);

        if (ssock < 0)
        {
            perror("Error: accept failed");
            exit(0);
        }
        cout << "client sockfd: " << ssock << endl;

        child_pid = fork();
        while (child_pid < 0)
        {
            child_pid = fork();
        }

        // slave process，用來服務特定用戶。
        if (child_pid == 0)
        {
            dup2(ssock, STDIN_FILENO);
            dup2(ssock, STDERR_FILENO);
            dup2(ssock, STDOUT_FILENO);
            close(ssock);
            close(msock);

            int client_id = GetClientIDFromSHM();
            AddNewUser(client_id, cl_addr);

            setenv("PATH", "bin:.", 1);

            // signal handler
            // 接收到其他user的message
            signal(SIGUSR1, SigHandler);
            // 其他user透過FIFO傳送user pipe input
            signal(SIGUSR2, SigHandler);
            // Terminate the process
            signal(SIGINT, SigHandler);
            signal(SIGQUIT, SigHandler);
            signal(SIGTERM, SigHandler);

            LoginUser(client_id);

            socketShell(client_id);

            LogoutUser(client_id);
        }
        // master process，用來接客，所以繼續接客
        else
        {
            close(ssock);
        }
    }
}