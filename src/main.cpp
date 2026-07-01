#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <functional>
#include <iostream>
#include <linux/limits.h>
#include <map>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>


bool should_exit = false;

using builtin_func_ptr = std::function<void(std::vector<std::string>&)>;

extern std::map<std::string, builtin_func_ptr> builtin_map;

void builtin_exit(std::vector<std::string>& args) {
  (void)args;
  should_exit = true;
}

void builtin_echo(std::vector<std::string>& args) {
  args.erase(args.begin());
  for (int i=0; i<args.size(); i++) {
    std::cout << args[i];
    if(i<args.size()-1){
      std::cout<<" ";
    }
  }
  std::cout << "\n";
}

void builtin_type(std::vector<std::string>& args) {
  args.erase(args.begin());
  for (auto& it : args) {
    auto f = builtin_map.find(it);
    if (f != builtin_map.end()) {
      std::cout << it << " is a shell builtin\n";
    } else {
      auto env_path = std::getenv("PATH");
      std::stringstream ss(env_path);
      std::string path = "";
      bool cmd_exists = false;
      while (getline(ss, path, ':')) {
        std::string full_path = path + '/' + it;
        if (access(full_path.c_str(), F_OK) == 0 && access(full_path.c_str(), X_OK) == 0) {
          cmd_exists = true;
          std::cout << it << " is " << full_path << "\n";
          break;
        }
      }
      if (!cmd_exists) {
        std::cout << it << ": not found\n";
      }
    }
  }
}

void builtin_pwd(std::vector<std::string>& args) {
  (void)args;
  char dir[PATH_MAX];
  if (getcwd(dir, sizeof(dir)) != NULL) {
    std::cout << dir << "\n";
  } else {
    std::cerr << "Error running getcwd()\n";
  }
}

void builtin_cd(std::vector<std::string>& args) {
  args.erase(args.begin());
  const char* env_home = std::getenv("HOME");
  if (args.size() > 1) {
    std::cerr << "cd: too many arguments\n";
  } else {
    const char* path = args[0].c_str();
    if (strcmp(path, "~") == 0) {
      path = env_home;
    }
    if (chdir(path) == -1) {
      std::cerr << "cd: " << path << ": No such file or directory\n";
    }
  }
}

std::map<std::string, builtin_func_ptr> builtin_map = {
  {"exit", builtin_exit},
  {"echo", builtin_echo},
  {"type", builtin_type},
  {"pwd", builtin_pwd},
  {"cd", builtin_cd}
};

std::vector<std::string> tokenizer(std::string cmd, bool in_single_quote = false, bool in_double_quote = false, bool in_token = false) {
  std::string curr_token = "";
  std::vector<std::string> tokens;
  int cmd_size = cmd.size();
  for (int i = 0; i < cmd_size; i++) {
    char c = cmd[i];
    if (c == '\\' && !in_single_quote) {
      if (in_double_quote) {
        if ((i + 1) < cmd.size() && (cmd[i + 1] == '$' || cmd[i + 1] == '\\' || cmd[i + 1] == '"')) {
          curr_token += cmd[++i];
        } else {
          curr_token += c;
        }
      } else {
        if (i + 1 < cmd.size()) {
          in_token = true;
          curr_token += cmd[++i];
        }
      }
      continue;
    }
    if (c == '"' && !in_single_quote) {
      if (in_double_quote) {
        in_double_quote = false;
      } else {
        in_double_quote = true;
      }
      in_token = true;
      continue;
    }

    if (c == '\'' && !in_double_quote) {
      if (in_single_quote) {
        in_single_quote = false;
      } else {
        in_single_quote = true;
      }
      in_token = true;
      continue;
    }
    if (c == ' ' && !in_double_quote && !in_single_quote) {
      if (in_token) {
        tokens.push_back(curr_token);
        curr_token = "";
        in_token = false;
      }
      continue;
    }
    in_token = true;
    curr_token += c;
    continue;
  }
  if (in_token) {
    tokens.push_back(curr_token);
  }

  if (in_single_quote || in_double_quote) {
    std::string cmd_new;
    std::cout << "> ";
    if (getline(std::cin, cmd_new)) {
      std::vector<std::string> temp_token = tokenizer("\n" + cmd_new, in_single_quote, in_double_quote, in_token);
      for (auto it : temp_token) {
        tokens.push_back(it);
      }
    }
  }
  return tokens;
}

std::pair<int, std::string> parse_redirection(std::vector<std::string>& args, bool& syntax_error) {
  std::string output_file = "";
  int target_fd = -1;
  for (int i = 0; i < args.size(); i++) {
    if (args[i] == ">" || args[i] == "1>" || args[i] == "2>" ||
        args[i] == ">>" || args[i] == "1>>" ||
        args[i] == "2>>") {
      if (args[i] == ">" || args[i] == "1>") {
        target_fd = 1;
      }
      if (args[i] == "2>") {
        target_fd = 2;
      }
      if (args[i] == ">>" || args[i] == "1>>") {
        target_fd = 3;
      }
      if (args[i] == "2>>") {
        target_fd = 4;
      }
      if (i + 1 >= args.size()) {
        std::cerr << "shell: Expected a path to a file";
        syntax_error = true;
        return {-1, ""};
      }
      output_file = args[i + 1];
      args.erase(args.begin() + i);
      args.erase(args.begin() + i);
    }
  }
  return {target_fd, output_file};
}
void internal_execution(std::vector<std::string> args, int inp_fd=-1, int out_fd=-1){
  bool syntax_error = false;
  std::pair<int, std::string> redirection_instruction = parse_redirection(args, syntax_error);
  if (syntax_error) {
    std::cerr << "Syntax Error\n";
    exit(1);
  }
  int saved_std = -1;
  int target = redirection_instruction.first;
  if (target != -1) {
    std::string output_file = redirection_instruction.second;
    int fd = open(output_file.c_str(), O_WRONLY | O_CREAT | (target >= 3 ? O_APPEND : O_TRUNC), 0644);
    if (fd < 0) {
      std::cerr << "Failed to open file\n";
      return;
    }
    if (target == 1 || target == 3) {
      saved_std = dup(STDOUT_FILENO);
      dup2(fd, STDOUT_FILENO);
    } else if (target == 2 || target == 4) {
      saved_std = dup(STDERR_FILENO);
      dup2(fd, STDERR_FILENO);
    }
    close(fd);
  }
  auto f=builtin_map.find(args[0]);
  f->second(args);
  if (target != -1) {
    if (target == 1 || target == 3) {
      dup2(saved_std, STDOUT_FILENO);
    } else if (target == 2 || target == 4) {
      dup2(saved_std, STDERR_FILENO);
    }
    close(saved_std);
  }

  return;
}


void external_execution(std::vector<std::string> args, int inp_fd = -1, int out_fd = -1) {
  bool syntax_error = false;
  std::pair<int, std::string> redirection_instruction = parse_redirection(args, syntax_error);
  if (syntax_error) {
    std::cerr << "Syntax Error\n";
    exit(1);
  }
  if (inp_fd != -1) {
    dup2(inp_fd, STDIN_FILENO);
    close(inp_fd);
  }
  if (out_fd != -1) {
    dup2(out_fd, STDOUT_FILENO);
    close(out_fd);
  }
  

  int saved_std = -1;

  int target = redirection_instruction.first;

  if (target != -1) {
    std::string output_file = redirection_instruction.second;

    int fd = open(output_file.c_str(), O_WRONLY | O_CREAT | (target >= 3 ? O_APPEND : O_TRUNC), 0644);

    if (fd < 0) {
      std::cerr << "Failed to open file\n";
      return;
    }

    if (target == 1 || target == 3) {
      saved_std = dup(STDOUT_FILENO);
      dup2(fd, STDOUT_FILENO);
    } else if (target == 2 || target == 4) {
      saved_std = dup(STDERR_FILENO);
      dup2(fd, STDERR_FILENO);
    }

    close(fd);
    }
    std::vector<char*> args_c;
    for (std::string& it : args) {
      args_c.push_back(&it[0]);
    }
    args_c.push_back(nullptr);
    if (execvp(args_c[0], args_c.data()) < 0) {
      perror(args_c[0]);
      exit(EXIT_FAILURE);
    }
    exit(0);
}

void loop() {
  std::cout << "$ ";

  std::string cmd;

  getline(std::cin, cmd);

  std::vector<std::string> args = tokenizer(cmd);

  if (args.empty()) {
    return;
  }

  bool should_pipe = 0;
  // for(auto& it:args){
  //   std::cout<<it<<" ";
  // }
  // std::cout<<std::endl;
  std::vector<std::vector<std::string>> cmd_grp;
  std::vector<std::string> temp;

  for (auto& it : args) {
    if (it.size() == 1 && it[0] == '|') {
      should_pipe = 1;
      cmd_grp.push_back(temp);
      temp.resize(0);
    } else {
      temp.push_back(it);
    }
  }

  if (!temp.empty()) {
    cmd_grp.push_back(temp);
  }

  // for (auto& it : cmd_grp) {
  //   for (auto& it2 : it) {
  //     std::cout << it2 << " ";
  //   }
  //   std::cout << std::endl;
  // }

  // if (!should_pipe) {
  //   command_execution(cmd_grp[0]);
  //   return;
  // }
  size_t max_pipes=cmd_grp.size()-1;
  int pipe_fd[max_pipes][2];
  std::vector<pid_t> pids;
  
  // pid_t p1=fork();
  // if(p1==0){
  //   close(pipe_fd[0]);
  //   command_execution(cmd_grp[0],-1,pipe_fd[1]);
  //   exit(0);
  // }
  // pid_t p2=fork();
  // if(p2==0){
  //   close(pipe_fd[1]);
  //   command_execution(cmd_grp[1],pipe_fd[0],-1);
  //   exit(0);
  // }
    for (size_t i = 0; i < cmd_grp.size(); i++) {
    // 1. Create pipes
    if (i < max_pipes && pipe(pipe_fd[i]) == -1) {
        perror("pipe");
        return;
    }

    int input = (i == 0) ? -1 : pipe_fd[i - 1][0];
    int output = (i == cmd_grp.size() - 1) ? -1 : pipe_fd[i][1];

    auto f = builtin_map.find(cmd_grp[i][0]);
    if (f == builtin_map.end()) {
        pid_t p = fork();
        if (p == 0) {
            // CHILD: Redirect and close all other pipe ends
            if (input != -1) { dup2(input, STDIN_FILENO); close(input); }
            if (output != -1) { dup2(output, STDOUT_FILENO); close(output); }
            
            for (int j = 0; j < max_pipes; j++) {
                close(pipe_fd[j][0]);
                close(pipe_fd[j][1]);
            }
            external_execution(cmd_grp[i], -1, -1);
            exit(0);
        } else {
            pids.push_back(p);
        }
    } else {
      internal_execution(cmd_grp[i],input,output);
    }
    if (i > 0) close(pipe_fd[i - 1][0]);      
    if (i < max_pipes) close(pipe_fd[i][1]);  
}
      for(auto& it:pids){
        int status;
        waitpid(it,&status,0);
      }
      
}

int main() {
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  while (!should_exit) {
    loop();
  }

  return 0;
}