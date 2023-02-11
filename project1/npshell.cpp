#include <iostream>
#include <unistd.h> 
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/wait.h>

using namespace std;

#define MAX_SINGLE_INPUT_LENGTH 15000
#define MAX_COMMAND_LENGTH 256
#define MAX_PIPES_NUMBER 1000
#define MAX_COMMAND_NUMBER 5000

struct inputSegmentTable {
    char *segmentation[MAX_SINGLE_INPUT_LENGTH];
    int length = 0;
    int index = 0;
};

enum pipeType 
{
    none,
    ordinary,
    numbered,
    numberedWithError
};

struct command
{
    int argc;
    char *argv[MAX_COMMAND_LENGTH];
    pipeType pipetype = none;
    int pipeNum = 0;    //需要預設嗎
    char *filename=NULL;
};

struct pipeFD
{
    int FDInOut[2];
    int countDown = -1;
    bool isNumPipe = false;
};

void initInputTable(inputSegmentTable &inputTable) {
    for (int i=0; i < inputTable.length; i++) {
        memset(inputTable.segmentation[i] ,'\0', sizeof(inputTable.segmentation[i]));
    }
    inputTable.length = 0;
    inputTable.index = 0;
}

void segmentInput(char *input, inputSegmentTable &inputTable){
    const char* d = " ";
    char *pch = strtok(input, d);
    while (pch != NULL) {
        inputTable.segmentation[inputTable.length++] = pch;
        pch = strtok(NULL, d);		   
    }
}


void initCommand(command &Command) {
    Command.argc = 0;
    memset(Command.argv ,'\0', sizeof(Command.argv));
    Command.pipetype = none;
    Command.pipeNum = 0;    //需要預設嗎
    Command.filename=NULL;
}

//看要執行什麼command(看第一個，eg. ls | cat | cat)
            //參數 cat test.html
            //停的時候：{ 1.後面沒東西（eg. 上例的最後一個cat）
            //          2.pipe |  |3  !2 
            //          3.file redirection (<),多read一個filename
            //          }
            //output: argv[], eg.cat text.txt
            //                =>[0]=cat [1]text.txt
            //        pipetype -none
            //                 -ordinary
            //                 -numbered
            //                 -nuberedWithError
            //        pipeNum, eg. |3 -> 3, !2 -> 2, 1 -> 1
            //        filename: 若出現 >,>的下一個
void parseCommand(inputSegmentTable &inputTable, command &Command){
    int currentIndex = 0;

    while(inputTable.index < inputTable.length){
        Command.argv[currentIndex++] = inputTable.segmentation[inputTable.index++];
        Command.argc++;

        if (inputTable.segmentation[inputTable.index] == NULL) {
            break;
        }

        if(strcmp(inputTable.segmentation[inputTable.index],"|") == 0){
            Command.pipetype = ordinary;
            Command.pipeNum = 1;
            inputTable.index++;
            break;
        }
        else if(inputTable.segmentation[inputTable.index][0] == '|'){ //不確定指標陣列
            Command.pipetype = numbered;
            Command.pipeNum = atoi(&inputTable.segmentation[inputTable.index][1]);
            inputTable.index++;
            break;
        }
        else if(inputTable.segmentation[inputTable.index][0] == '!'){
            Command.pipetype = numberedWithError;
            Command.pipeNum = atoi(&inputTable.segmentation[inputTable.index][1]);
            inputTable.index++;
            break;
        }
        else if(inputTable.segmentation[inputTable.index][0] == '>'){
            
            Command.filename = inputTable.segmentation[++inputTable.index];
            inputTable.index++;
            break;
        }
    }
}

void countDownNumPipeRoutine(pipeFD *pipeFDs, int &pipeLength){
    for (int i = 0; i < pipeLength; i++) {
        if(pipeFDs[i].isNumPipe == true){
            pipeFDs[i].countDown -= 1;
        }
    }
}

void countDownOrdinaryPipeRoutine(pipeFD *pipeFDs, int &pipeLength){
    for (int i = 0; i < pipeLength; i++) {
        if(pipeFDs[i].isNumPipe == false){
            pipeFDs[i].countDown -= 1;
        }
    }
}

int findInReplace(pipeFD *pipeFDs, int &pipeLength){
    for(int i=0; i < pipeLength; i++){
        if(pipeFDs[i].countDown == 0){
            return i;
        }
    }
    return -1;
}

int findOutReplace(int pipeNum, pipeFD *pipeFDs, int &pipeLength, pipeType pipetype){
    for(int i=0; i < pipeLength; i++){
        if(pipeFDs[i].countDown == pipeNum){
            if(pipetype == ordinary){
                if(pipeFDs[i].isNumPipe == false){
                    return i;
                }
            }
            else{
                if(pipeFDs[i].isNumPipe == true){
                    return i;
                }
            }
        }
    }
    return -1;
}

//             從pipeTable找countdown = 0
//             如果找不到 => STDIN = 0
int createInputPipe(pipeFD *pipeFDs, command Command, int &pipeLength){
    if(findInReplace(pipeFDs, pipeLength)!=-1){
        close(pipeFDs[findInReplace(pipeFDs,pipeLength)].FDInOut[1]);
        //代表此Command之Iutput為此pipe讀出之Output(FDInOut[0])
        return pipeFDs[findInReplace(pipeFDs,pipeLength)].FDInOut[0];   //not knowing my struct define FDInOut as pointer is right or not
    }
    else{
        //STDIN
        return STDIN_FILENO;
    }
    
}

void createPipe(pipeFD *pipeFDs, int countDown, pipeType pipetype, int &pipeLength) {
    pipe(pipeFDs[pipeLength].FDInOut);
    pipeFDs[pipeLength].countDown = countDown;
    if( pipetype == numbered || pipetype == numberedWithError ){
        pipeFDs[pipeLength].isNumPipe = true;
    } else {
        ////!!!!!!!!!!!!!!!!!!!!!!!!!
        pipeFDs[pipeLength].isNumPipe = false;
    }
    pipeLength++;
}

//             if(filename 有值)->fopen()
//             if(pipetype = name)->STDOUT
//             if pipetype = ordinary, numbered, numberedWithError
//                        利用pipeNum找有沒有countdown = pipeNUm, 有就return
//                        沒有時需create pipe,並將其放入pipeTable
int createOutputPipe(pipeFD *pipeFDs, command Command,int &pipeLength){
    if(Command.filename != NULL){
        int p = open(Command.filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
        return p;
    }

    if(Command.pipetype == none){
        //STDOUT
        return STDOUT_FILENO;
    }
    else{
        if(findOutReplace(Command.pipeNum, pipeFDs, pipeLength, Command.pipetype)!= -1){
            //代表此Command之Output為寫入此pipe之input(FDInOut[1])
            return pipeFDs[findOutReplace(Command.pipeNum,pipeFDs,pipeLength, Command.pipetype)].FDInOut[1];   //not knowing my struct define FDInOut as pointer is right or not
        }
        else{
            //pipeCreate
            createPipe(pipeFDs, Command.pipeNum, Command.pipetype, pipeLength);
            return pipeFDs[pipeLength-1].FDInOut[1];
        }
    }
        
}

//             if(pipetype = numberedWithError)
//                        利用pipeNum找有沒有countdown = pipeNUm, 有就return
//                        沒有時需create pipe,並將其放入pipeTable
int createErrorPipe(pipeFD *pipeFDs, command Command,int &pipeLength){
    if(Command.pipetype == numberedWithError){
        if(findOutReplace(Command.pipeNum, pipeFDs, pipeLength, Command.pipetype)!= -1){
            //代表此Command之Error為寫入此pipe之input(FDInOut[1])
            return pipeFDs[findOutReplace(Command.pipeNum,pipeFDs,pipeLength, Command.pipetype)].FDInOut[1];   //not knowing my struct define FDInOut as pointer is right or not
        }
        else{
            //pipeCreate
            createPipe(pipeFDs, Command.pipeNum, Command.pipetype, pipeLength);
            return pipeFDs[pipeLength-1].FDInOut[1];
        }
    } else {
        return STDERR_FILENO;
    }
}

void getCmdFilePath(command Command, char cmdFilePath[]){
    if((strcmp(Command.argv[0], "setenv")) == 0){
        strcpy(cmdFilePath, "setenv");
        return;
    }
    else if((strcmp(Command.argv[0], "printenv")) == 0){
        strcpy(cmdFilePath, "printenv");
        return;
    }
    else if((strcmp(Command.argv[0], "exit")) == 0){
        strcpy(cmdFilePath, "exit");
        return;
    }
    else{
        char *envPath = getenv("PATH");
        //將環境變數作切割,分別去各個環境變數找與Command.argv[0]相同名稱的執行擋
        char envPathTodo[MAX_COMMAND_LENGTH];
        strcpy(envPathTodo, envPath);
        char delim[] = ":";
        char *pch = strtok(envPathTodo, delim);

        while (pch != NULL) {
            strcpy(cmdFilePath, pch);
            char *fullFilePath = strcat(strcat(cmdFilePath, "/"), Command.argv[0]);
            FILE *fp = fopen(fullFilePath, "r");
            if (fp) {
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

void ChildHandler(int signo) {
    int status;
    // https://baike.baidu.hk/item/waitpid/4071590
    while (waitpid(-1, &status, WNOHANG) > 0) {

    }
}

void executeCommand(command Command, char *cmdFilePath, pid_t pid_table[], int &pid_length, int inputfd, int outputfd, int errfd){
    if (!strcmp(cmdFilePath, "exit")) {
        exit(0);
    }
    else if (!strcmp(cmdFilePath, "setenv")) {
        setenv(Command.argv[1], Command.argv[2], 1);
    }
    else if (!strcmp(cmdFilePath, "printenv")) {
        char *msg = getenv(Command.argv[1]);
        if (msg) {
            cout << msg << endl;
        }
    }
    else {
        //1.fork() child process
        //2.再dup(),後將設定好的in,out errPipe接上
        //3.執行execlp(叫出cmdFilePath中的執行檔)
        signal(SIGCHLD, ChildHandler);
        pid_t c_pid = fork();

        //if fork error
        while(c_pid < 0){
            int status;
            waitpid(-1, &status, 0);
            c_pid = fork();
        }

        if(c_pid == 0){
            //子
            if(inputfd != STDIN_FILENO){
                dup2(inputfd, STDIN_FILENO);
                close(inputfd);
            }
            if(outputfd != STDOUT_FILENO){
                dup2(outputfd, STDOUT_FILENO);
            }
            if(errfd != STDERR_FILENO){
                dup2(errfd, STDERR_FILENO);
            }
            if(outputfd != STDOUT_FILENO){
                close(outputfd);
            }
            if(errfd != STDERR_FILENO){
                close(errfd);
            }
            if(!strcmp(cmdFilePath, "")){
                cerr << "Unknown command: [" << Command.argv[0] << "]." << endl;
            } else {
                execvp(cmdFilePath, Command.argv);
            }
            exit(0);
        }
        else{
            //父
            //等待child process完成
            //!//可以wait的時間點
            if (Command.pipetype == none || Command.filename != NULL) {
                pid_table[pid_length++] = c_pid;
                for (int i = 0; i < pid_length; ++i) {
                    int status;
                    waitpid(pid_table[i], &status, 0);
                }
                if (Command.filename != NULL) {
                    close(outputfd);
                }
            } else {
                pid_table[pid_length++] = c_pid;
            }

        }
    }
}

void closePipefd(pipeFD *pipeFDs, int &pipeLength) {
    for(int i = 0; i < pipeLength; i++ ){
        if(pipeFDs[i].countDown <= 0){
            close(pipeFDs[i].FDInOut[0]);
            pipeFDs[i].FDInOut[0] = -1;

            pipeLength -= 1;

            if(pipeLength > 0){
                pipeFD temp = pipeFDs[pipeLength];
                pipeFDs[pipeLength] = pipeFDs[i];
                pipeFDs[i] = temp;
            }

            i -= 1;
        }
    }
}


int main(int argc, char*argv[]){
    
    //initPath, setenv()
    setenv("PATH", "bin:.", 1);

    char input[MAX_SINGLE_INPUT_LENGTH];
    memset(&input ,'\0', sizeof(input));
    pipeFD pipeFDs[MAX_PIPES_NUMBER];
    int pipeLength = 0;
    pid_t pid_table[MAX_COMMAND_NUMBER]; ////!////
    int pid_length = 0;
    char cmdFilePath[MAX_COMMAND_LENGTH];
    bool lastCommandIsNum = false;
    inputSegmentTable inputTable;

    while(true) {
        cout << "% ";            
        char input[MAX_SINGLE_INPUT_LENGTH];

        memset(&input ,'\0', sizeof(input));
        initInputTable(inputTable);

        if(!cin.getline(input,sizeof(input))){
            break;
        }

        if(!strcmp(input,"\0")){
            continue;
        }
        
        segmentInput(input, inputTable);

        /////////////////////!!!!!!!!!!!!!!!!
        // Handle numbered pipe for new line input command.
        countDownNumPipeRoutine(pipeFDs, pipeLength);
        lastCommandIsNum = false;
        /////////////////////!!!!!!!!!!!!!!!!

        //inputTable.length:一行指令的command個數
        while(inputTable.index < inputTable.length) {
            command Command;
            int inputfd, outputfd, errfd;

            // Init cmdFilePath
            memset(&cmdFilePath ,'\0', sizeof(cmdFilePath));

            initCommand(Command);
            parseCommand(inputTable,Command);

            /////////////////////!!!!!!!!!!!!!!!!
            // Handle numbered pipe in middle.
            if (lastCommandIsNum) {
                countDownNumPipeRoutine(pipeFDs, pipeLength);
                lastCommandIsNum = false;
            } else {
                // TODO: Think the logic.
                countDownOrdinaryPipeRoutine(pipeFDs, pipeLength);
            }
            
            if(Command.pipetype == numbered || Command.pipetype == numberedWithError){
                lastCommandIsNum = true;
            }
            /////////////////////!!!!!!!!!!!!!!!!
            
            //FDTable index
            inputfd = createInputPipe(pipeFDs, Command, pipeLength);
            outputfd = createOutputPipe(pipeFDs, Command, pipeLength);
            errfd = createErrorPipe(pipeFDs, Command, pipeLength);

            getCmdFilePath(Command, cmdFilePath);
            
            ///!///
            executeCommand(Command, cmdFilePath, pid_table, pid_length, inputfd, outputfd, errfd);
            
            closePipefd(pipeFDs, pipeLength);
        }
    }
}