#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <sstream>
#include <functional>
#include <map>
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <linux/limits.h>

bool should_exit = false; 
using builtin_func_ptr = std::function<void(std::vector<std::string>&)>;
extern std::map<std::string,builtin_func_ptr> builtin_map;


void builtin_exit(std::vector<std::string>& args){
  should_exit=true;
}

void builtin_echo(std::vector<std::string>& args){
  args.erase(args.begin());
  for(auto& it:args){
    std::cout<<it<<" ";
  }
  std::cout<<"\n";
}

void builtin_type(std::vector<std::string>& args){
  args.erase(args.begin());

  for(auto& it:args){
    auto f=builtin_map.find(it);
    if(f!=builtin_map.end()){
      std::cout<<it<<" is a shell builtin\n";
    }
    else{
      auto env_path=std::getenv("PATH");
      std::stringstream ss(env_path);
      std::string path="";
      bool cmd_exists=false;
      while(getline(ss,path,':')){
        std::string full_path= path+'/'+it;
        if(access(full_path.c_str(),F_OK)==0 && 
          access(full_path.c_str(),X_OK)==0){
          cmd_exists=true;
          std::cout<<it<<" is "<<full_path<<"\n";  
          break;
        }
      }
      if(!cmd_exists)
        std::cout<<it<<": not found\n";
    }
  }
}

void builtin_pwd(std::vector<std::string>& args){
  char dir[PATH_MAX];
  if(getcwd(dir,sizeof(dir))!=NULL){
    std::cout<<dir<<"\n";
  }
  else{
    std::cerr<<"Error running getcwd()\n";
  }
}

void builtin_cd(std::vector<std::string>& args){
  args.erase(args.begin());
  const char* env_home=std::getenv("HOME");
  if(args.size()>1){
    std::cerr<<"cd: too many arguments\n";
  }
  else{
    const char* path=args[0].c_str();
    if(strcmp(path,"~")==0){
     path=env_home; 
    }
    if(chdir(path)==-1){
      std::cerr<<"cd: "<<path<<": No such file or directory\n";
    }
  }
}

std::map<std::string,builtin_func_ptr> 
  builtin_map= { 
    {"exit",builtin_exit},
    {"echo",builtin_echo},
    {"type",builtin_type},
    {"pwd",builtin_pwd},
    {"cd",builtin_cd}
  };

std::vector<std::string> tokenizer(std::string cmd){
  bool  in_single_quote=false,
        in_double_quote=false,
        in_token=false;
  std::string curr_token="";
  std::vector<std::string> tokens;
  int cmd_size=cmd.size();
  for(int i=0; i<cmd_size; i++){
    char c=cmd[i];
    
    if(c=='"' && !in_single_quote){
      if(in_double_quote){
        in_double_quote=false;
      }
      else{
        in_double_quote=true;
      }
      in_token=true;
      continue;
    }
    if(c=='\'' && !in_double_quote){
      if(in_single_quote){
        in_single_quote=false;
      }
      else{
        in_single_quote=true;
      }
      in_token=true;
      continue;
    }
    if(c==' ' && !in_double_quote && !in_single_quote){
      if(in_token){
        tokens.push_back(curr_token);
        curr_token="";
        in_token=false;
      }
      continue;
    }
    in_token=true;
    curr_token+=c;
    continue;
  }
  if(in_token){
    tokens.push_back(curr_token);
  }
  return tokens;
}

void loop(){
  std::cout << "$ ";
  std::string cmd;
  std::getline(std::cin, cmd);
  // std::stringstream ss(cmd);
  
  // std::string arg;
  // while(ss>>arg){
    //     args.push_back(arg);
    // }
  std::vector<std::string> args=tokenizer(cmd);
  // for(auto& it:args){
  //   std::cout<<it<<std::endl;
  // }
  if(args.empty()) {
    return;
  }
  
  auto f=builtin_map.find(args[0]);
  if(f!=builtin_map.end()){
    f->second(args);
    return;
  }
  
  pid_t p=fork();
  int status;
  if(p==-1){
    std::cerr<<"Failure\n";
    exit(1);
  }
  else if(p==0){
    std::vector<const char*> args_c;
    for(const std::string& it:args){
      args_c.push_back(it.c_str());
    }
    args_c.push_back(nullptr);
    if(execvp(args_c[0], const_cast<char* const*>(args_c.data()))<0){
      std::cerr<<args_c[0]<<": command not found\n";
      exit(EXIT_FAILURE);
    }
    exit(0);
  }
  else{
    waitpid(p,&status,0);
  }
}

int main() {
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  while(!should_exit){
      loop();
  }

  return 0;
}
