# Linux Terminal Based File Explorer

This is a fully functional File Explorer Application that runs on Linux machines with a restricted feature set. </br>
Prerequisite - GCC/G++ compiler needs to be pre-installed. </br>
Download the source code and run the following commands to start the application: 
```
g++ main.cpp 
./a.out
```

##

<b>It works in two modes :</b>
### 1. Normal mode (default mode) 
Navigate the filesystem by arrow/backspace/enter keys.
### 2. Command mode 
Perform operations and navigate via entering shell commands.

##

**Detailed features of the Normal mode:**
- Get the details(file name/file size/owner/last modified) listed of all the files/folders in the current directory. 
- '.' represents current directory and '..' represents parent directory
- Navigate via up and down arrow keys.  
- User can go back to the previously visited directory or the next directory via left and right arrow keys.
- Go one level up to the parent directory using Backspace.
- Go to the home folder by 'h' key.
- Enter command mode by pressing colon i.e. ':'

**Various commands of the Command mode:**
- goto, create file/directory, delete file/directory, copy, move, rename, search 
- Go back to normal mode by pressing 'Esc' key.

**Closing Application:**
- Normal mode: 'q' key
- Command mode: 'quit' command

## Working demo:

[Screencast from 27-12-22 02:39:48 AM IST.webm](https://user-images.githubusercontent.com/53634655/209583657-bad23c19-aab8-4330-bae9-46f7cba9d9c3.webm)

