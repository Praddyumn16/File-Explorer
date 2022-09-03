#include <bits/stdc++.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <iostream>
#include <termios.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <cstdio>
using namespace std;

#define goto(x, y) printf("\033[%d;%dH", (y), (x))
#define K 20

/*global variables*/
stack<string> lft; // for left arrow
stack<string> rgt; // for right arrow
vector<string> files_and_dirs;
unordered_map<string, int> is_file_or_dir; // 0 for file , 1 for directory
DIR *dir;
struct dirent *ent;
struct termios raw;
struct stat check;
int CURSOR_TRACKER = 1;
bool raw_set = 0, isAbsolute = 0, command_set = 0;
string curr_dir = "";

void ENABLE_SCROLLING(int, int, int, int, int);
void PRINT_K_FILES_WITH_METADATA(int, int, int, int, int);
void START_NORMAL_MODE(string);
void GET_FILES(string);
void disableRawMode();

void clear_line()
{
    cout << "\33[2K\r";
    cout << "COMMAND MODE:: ";
}

string permissions(char *file)
{
    struct stat st;
    string ans = "";
    mode_t perm = st.st_mode;
    string s(file);
    ans += (is_file_or_dir[s] == 1) ? 'd' : '-';
    ans += (perm & S_IRUSR) ? 'r' : '-';
    ans += (perm & S_IWUSR) ? 'w' : '-';
    ans += (perm & S_IXUSR) ? 'x' : '-';
    ans += (perm & S_IRGRP) ? 'r' : '-';
    ans += (perm & S_IWGRP) ? 'w' : '-';
    ans += (perm & S_IXGRP) ? 'x' : '-';
    ans += (perm & S_IROTH) ? 'r' : '-';
    ans += (perm & S_IWOTH) ? 'w' : '-';
    ans += (perm & S_IXOTH) ? 'x' : '-';
    return ans;
}

void PRINT_DETAILS(string path)
{

    size_t found;
    char ch = '/';
    found = path.find_last_of(ch);

    string pathname;

    if (found == string::npos)
        pathname = path;
    else
        pathname = path.substr(found + 1);

    int len = pathname.size();
    if (len <= 10)
    {
        cout << pathname;
        string blanks(15 - len, ' ');
        cout << blanks << "\t";
    }
    else
    {
        for (int i = 0; i < 10; i++)
            cout << pathname[i];
        string dot(5, '.');
        cout << dot << "\t";
    }

    cout << permissions(path.data()) << "\t";

    struct stat info;
    int rc = stat(path.c_str(), &info); // Error check omitted
    struct passwd *pw = getpwuid(info.st_uid);
    struct group *gr = getgrgid(info.st_gid);
    cout << pw->pw_name << "\t";
    cout << gr->gr_name << "\t";

    string byte_size = to_string(info.st_size / 1024) + " KB";
    len = byte_size.size();
    if (len <= 10)
    {
        cout << byte_size;
        string blanks(15 - len, ' ');
        cout << blanks << "\t";
    }
    else
    {
        for (int i = 0; i < 10; i++)
            cout << byte_size[i];
        string dot(5, '.');
        cout << dot << "\t";
    }
    cout << std::ctime(&info.st_ctime);
    // cout << endl;
}

string identify_command(string command)
{
    string res = "";
    for (auto a : command)
    {
        if (a == ' ')
            break;
        res += a;
    }
    return res;
}

bool search(string curr, string path)
{

    DIR *dir1;
    struct dirent *ent1;
    struct stat check1;
    bool ans = false;

    // cout << curr << endl;

    dir1 = opendir(curr.c_str());
    while ((ent1 = readdir(dir1)) != NULL)
    {
        string x = ent1->d_name;
        if (x == path) // found the file
        {
            closedir(dir1);
            return true;
        }
        if (x[0] == '.') // do not look into hidden files or infinite loop of current directory
            continue;

        string new_curr = curr + "/" + x;
        if (stat(new_curr.c_str(), &check1) != 0)
            continue;

        if (check1.st_mode & S_IFDIR) // it's a directory
        {
            if (search(new_curr, path))
            {
                closedir(dir1);
                return true;
            }
        }
    }
    closedir(dir1);
    return false;
}

void PRINT_K_FILES_WITH_METADATA_CMD(int curr, int start, int end, int list_start, int list_end)
{

    /* clear screen */
    cout << "\033[2J\033[1;1H";

    // /* printing the details of all the files */
    for (int i = start - 1; i <= end - 1; i++)
    {
        PRINT_DETAILS(files_and_dirs[i]);
    }

    struct winsize max;
    ioctl(0, TIOCGWINSZ, &max);
    printf("\033[%d;%dH", max.ws_row, 1);

    cout << "COMMAND MODE:: ";
}

void tokenize(string command, vector<string> &v, char c)
{
    string temp = "";
    for (auto a : command)
    {
        if (a == c)
        {
            v.push_back(temp);
            temp = "";
        }
        else
            temp += a;
    }
    v.push_back(temp);
}

string rel_to_absolute(string rel_path)
{
    char ch = rel_path[0];
    if (ch == '/')
        return rel_path;

    if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' || ch <= 'z') && (ch != '~')) // it's already an absolute path
    {
        return curr_dir + "/" + rel_path;
    }

    vector<string> v;
    tokenize(rel_path, v, '/');

    if (v[0] == "." || v[0] == "..")
        return (curr_dir + "/" + rel_path);

    struct passwd *pw;
    uid_t uid;
    uid = geteuid();
    pw = getpwuid(uid);
    string x = pw->pw_name;
    string abs_path = "/home/" + x;
    if (rel_path.size() <= 2)
        return abs_path;
    else
        return abs_path + "/" + rel_path.substr(2);
}

void MOVE(string original_path, string new_path)
{
    char *c = const_cast<char *>(original_path.c_str());
    char *d = const_cast<char *>(new_path.c_str());

    rename(c, d);
}

void COPY_FILE(string src_path, string dest_path)
{
    int in = open(src_path.c_str(), O_RDONLY);
    int out = open(dest_path.c_str(), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
    char block_size[1024];
    int x;
    while (x = read(in, block_size, 1024) > 0)
        write(out, block_size, x);

    struct stat sStat;
    stat(src_path.c_str(), &sStat);

    chown(dest_path.c_str(), sStat.st_uid, sStat.st_gid);
    chmod(dest_path.c_str(), sStat.st_mode);
}

void COPY_DIRECTORY(string src_path, string dest_dir_path)
{
    // folder = folder to be copied;
    string folder = "";
    for (int i = src_path.size() - 1; i >= 0; i--)
    {
        if (src_path[i] == '/')
            break;
        folder += src_path[i];
    }
    reverse(folder.begin(), folder.end());

    string final_dest_path = dest_dir_path + "/" + folder;
    mkdir(final_dest_path.c_str(), S_IRWXO | S_IRUSR | S_IWUSR | S_IXUSR);

    DIR *dir1;
    struct dirent *ent1;
    dir1 = opendir(src_path.c_str());
    while ((ent1 = readdir(dir1)) != NULL)
    {
        string curr = ent1->d_name;
        if (curr == "." || curr == "..")
            continue;

        string temp = src_path + "/" + curr;
        struct stat check2;
        stat(temp.c_str(), &check2);

        if (check2.st_mode & S_IFDIR)
            COPY_DIRECTORY(temp, final_dest_path);
        else
            COPY_FILE(temp, final_dest_path + "/" + curr);
    }
    closedir(dir1);
}

void START_COMMAND_MODE()
{

    struct winsize max;
    ioctl(0, TIOCGWINSZ, &max);
    printf("\033[%d;%dH", max.ws_row, 1);
    cout << "COMMAND MODE:: ";
    clear_line();

    string command = "";

    while (true)
    {

        char c = getchar();
        if (c == 27)
            return;

        else if (c == 10)
        { // enter
            clear_line();
            string temp = identify_command(command);
            if (temp == "quit")
            {
                cout << "\033[2J\033[1;1H";
                exit(1);
            }
            else if (temp == "goto")
            {
                string path = "";
                bool flag = 0;
                for (auto a : command)
                {
                    if (a == ' ')
                        flag = 1;
                    if (flag)
                        path += a;
                }
                path = path.substr(1);
                dir = opendir(path.c_str());
                command = "";
                if (dir == NULL)
                    cout << "Invalid directory";
                else
                {
                    command_set = 1;
                    curr_dir = path;
                    closedir(dir);
                    GET_FILES(curr_dir);
                    int n = files_and_dirs.size();
                    PRINT_K_FILES_WITH_METADATA_CMD(1, 1, min(K, n), 1, n);
                }
            }
            else if (temp == "search")
            {
                string path = "";
                bool flag = 0;
                for (auto a : command)
                {
                    if (a == ' ')
                        flag = 1;
                    if (flag)
                        path += a;
                }
                path = path.substr(1);
                cout << (search(curr_dir, path) ? "Yes it exists" : "Does not exist");
                command = "";
            }
            else if (temp == "copy")
            {
                vector<string> v;
                tokenize(command, v, ' ');
                string destination_path = v[v.size() - 1];
                destination_path = rel_to_absolute(destination_path);

                for (int i = 1; i < v.size() - 1; i++)
                {
                    string original_path = curr_dir + "/" + v[i];
                    struct stat check1;
                    stat(original_path.data(), &check1) == 0;

                    if (check1.st_mode & S_IFDIR) // copy directory
                    {
                        COPY_DIRECTORY(original_path, destination_path);
                    }
                    else // copy file{
                    {
                        string temp_dest_path = destination_path + "/" + v[i];
                        COPY_FILE(original_path, temp_dest_path);
                    }
                }
                command = "";
            }
            else if (temp == "move")
            {
                vector<string> v;
                tokenize(command, v, ' ');
                string destination_path = v[v.size() - 1];
                destination_path = rel_to_absolute(destination_path);
                for (int i = 1; i < v.size() - 1; i++)
                {
                    string original_path = curr_dir + "/" + v[i];

                    struct stat check1;
                    stat(original_path.data(), &check1) == 0;

                    string temp_dest_path = destination_path + "/" + v[i];
                    MOVE(original_path, temp_dest_path);
                }

                command = "";
            }
            else if (temp == "rename")
            {
                vector<string> v;
                tokenize(command, v, ' ');
                string original_path = curr_dir + "/" + v[1], new_path = curr_dir + "/" + v[2];

                char *c = const_cast<char *>(original_path.c_str());
                char *d = const_cast<char *>(new_path.c_str());
                if (rename(c, d) != 0)
                    cout << "Error renaming file";
                else
                    cout << "Renamed successfully";
                command = "";
            }
            else if (temp == "create_file")
            {
                vector<string> v;
                tokenize(command, v, ' ');

                if (v.size() != 3)
                    cout << "Invalid command syntax";
                else
                {
                    string dir_path = rel_to_absolute(v[2]) + "/";
                    dir_path += v[1];
                    int status = open(dir_path.c_str(), O_RDONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                }
                command = "";
            }
            else if (temp == "create_dir")
            {
                vector<string> v;
                tokenize(command, v, ' ');

                if (v.size() != 3)
                    cout << "Invalid command syntax";
                else
                {
                    string dir_path = rel_to_absolute(v[2]) + "/";
                    dir_path += v[1];
                    int status = mkdir(dir_path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
                }
                command = "";
            }
            else if (temp == "delete_file")
            {
                vector<string> v;
                tokenize(command, v, ' ');
                string file_path = curr_dir + "/" + v[1];
                remove(file_path.c_str());
            }
            else if (temp == "delete_dir")
            {
            }
            else // invalid command
            {
                command = "";
                cout << "Invalid command";
            }
        }
        else if (c == 127)
        { // backspace
            int n = command.size();
            // printf("\033[%d;%dH", max.ws_row, 1);
            clear_line();
            command = command.substr(0, n - 1);
            cout << command;
        }
        else
        {
            command += c;
            cout << c;
        }
    }
}

void ENABLE_SCROLLING(int curr, int start, int end, int list_start, int list_end)
{

    /* setting the cursor on (0 , 0) */
    // cout << "\033[0;0H";
    printf("\033[%d;%dH", curr, 1);

    char c = '0', prev = '0', bprev = '0';

    while (c = getchar())
    {
        if (c == 'q')
        {
            cout << "\033[2J\033[1;1H";
            // disableRawMode();
            exit(1);
        }
        else if (c == 'h') // home folder
        {
            curr_dir = "/home";
            CURSOR_TRACKER = 1;
            // GET_FILES(curr_dir);
            // PRINT_K_FILES_WITH_METADATA(1, start, end, list_start, list_end);
            START_NORMAL_MODE(curr_dir);
        }
        else if (c == 127) // backspace
        {
            CURSOR_TRACKER = 1;
            int count = 0;
            for (auto a : curr_dir)
                if (a == '/')
                    count++;

            int curr = 0;
            string temp = "";
            for (auto a : curr_dir)
            {
                if (a == '/')
                    curr++;
                if (curr == count)
                    break;
                temp += a;
            }

            curr_dir = temp;
            START_NORMAL_MODE(curr_dir);
        }
        else if (c == 10) // pressed enter
        {
            if (is_file_or_dir[files_and_dirs[CURSOR_TRACKER - 1]] == 0) // it's a file
            {
                string path = files_and_dirs[CURSOR_TRACKER - 1]; // file's path name
                int pid = fork();
                if (pid == 0)
                {
                    execlp("/usr/bin/vi", "vi", path.c_str(), (char *)0);
                    exit(1);
                }
                else
                {
                    int val, status = 0;
                    while ((val = waitpid(pid, &status, 0)) != pid)
                    {
                        if (val == -1)
                            exit(1);
                    }
                }
                // execlp("xdg-open", "xdg-open", path.c_str(), (char *)0);
            }
            else // it's a directory
            {
                lft.push(curr_dir);
                curr_dir = files_and_dirs[CURSOR_TRACKER - 1];
                CURSOR_TRACKER = 1;
                START_NORMAL_MODE(curr_dir);
            }
        }
        else if (c == ':') // enter command mode
        {
            START_COMMAND_MODE();
            int n = files_and_dirs.size();
            PRINT_K_FILES_WITH_METADATA(1, 1, min(K, n), 1, n);
        }
        if (bprev == 27 && prev == 91 && c == 65)
        { // up arrow
            if (curr > 1 && curr <= min(K, list_end))
            {
                CURSOR_TRACKER--;
                curr--;
                printf("\033[%d;%dH", curr, 1);
            }
            else
            {
                if (start != list_start)
                {
                    start--;
                    end--;
                    CURSOR_TRACKER--;
                    PRINT_K_FILES_WITH_METADATA(1, start, end, list_start, list_end);
                }
            }
        }
        if (bprev == 27 && prev == '[' && c == 'B')
        { // down arrow
            if (curr >= 1 && curr < min(K, list_end))
            {
                CURSOR_TRACKER++;
                curr++;
                printf("\033[%d;%dH", curr, 1);
            }
            else
            {
                if (end != list_end)
                {
                    start++;
                    end++;
                    CURSOR_TRACKER++;
                    PRINT_K_FILES_WITH_METADATA(min(K, list_end), start, end, list_start, list_end);
                }
            }
        }
        if (bprev == 27 && prev == '[' && c == 'C')
        { // right arrow
            if (rgt.size() >= 1)
            {
                lft.push(curr_dir);
                curr_dir = rgt.top();
                rgt.pop();
                CURSOR_TRACKER = 1;
                START_NORMAL_MODE(curr_dir);
            }
        }
        if (bprev == 27 && prev == '[' && c == 'D')
        { // left arrow
            if (lft.size() >= 1)
            {
                rgt.push(curr_dir);
                curr_dir = lft.top();
                lft.pop();
                CURSOR_TRACKER = 1;
                START_NORMAL_MODE(curr_dir);
            }
        }
        bprev = prev;
        prev = c;
    }
}

void PRINT_K_FILES_WITH_METADATA(int curr, int start, int end, int list_start, int list_end)
{

    /* clear screen */
    cout << "\033[2J\033[1;1H";

    // /* printing the details of all the files */
    for (int i = start - 1; i <= end - 1; i++)
    {
        PRINT_DETAILS(files_and_dirs[i]);
    }

    struct winsize max;
    ioctl(0, TIOCGWINSZ, &max);
    printf("\033[%d;%dH", max.ws_row, 1);

    cout << "NORMAL MODE:: " << curr_dir;

    ENABLE_SCROLLING(curr, start, end, list_start, list_end);
}

void GET_FILES(string pwd)
{

    /* clear the clutter */
    files_and_dirs.clear();
    is_file_or_dir.clear();

    /* converting the pwd from string to char array */
    char arr[pwd.size() + 1];
    strcpy(arr, pwd.c_str());

    /* storing the list of files and directories of pwd in a vector */
    dir = opendir(arr);
    while ((ent = readdir(dir)) != NULL)
        files_and_dirs.push_back(curr_dir + "/" + ent->d_name);

    /* storing whether the item is a file or directory */
    for (int i = 0; i < files_and_dirs.size(); i++)
    {
        if (stat(files_and_dirs[i].data(), &check) == 0)
        {
            if (check.st_mode & S_IFDIR)
                is_file_or_dir[files_and_dirs[i]] = 1;
            else
                is_file_or_dir[files_and_dirs[i]] = 0;
        }
    }

    closedir(dir);
}

void enableRawMode()
{
    raw_set = 1;
    atexit(disableRawMode);
    tcgetattr(STDIN_FILENO, &raw);
    struct termios newmode;
    newmode = raw;
    newmode.c_lflag &= ~(ECHO);
    newmode.c_lflag &= ~(ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &newmode);
}

void disableRawMode()
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void START_NORMAL_MODE(string pwd)
{

    if (!raw_set)
        enableRawMode();

    GET_FILES(pwd);
    int n = files_and_dirs.size();

    if (n >= K)
        PRINT_K_FILES_WITH_METADATA(1, 1, K, 1, n);
    else
        PRINT_K_FILES_WITH_METADATA(1, 1, n, 1, n);

    disableRawMode();
}

int main()
{

    string pwd = get_current_dir_name();
    curr_dir = pwd;
    START_NORMAL_MODE(pwd);

    return 0;
}
// Left - delete directory