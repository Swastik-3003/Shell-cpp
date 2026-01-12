#include <cstdlib>
#include <bits/stdc++.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
using namespace std;

void print_v(vector<string> v){
  for(auto it : v){
    cout << it << " ";
  }
  cout << "\n";
}

void type_exec(vector<string>& array){
  const char* paths = getenv("PATH");
  //storing paths in an array
  string temp = "";
  vector<string> paths_array;
  for(int i=0; paths[i]!='\0'; i++){
    if(paths[i]==':'){
      paths_array.push_back(temp);
      temp = "";
      continue;
    }
    temp+=paths[i];
  }
  //checking for each command by adding it to the end of each path
  for(auto arg : array){
    if(arg == "type" || arg == "exit" || arg == "echo"){
      cout << arg << " is a shell builtin\n";
      break;
    }
    bool path_exists = false;
    for(auto path : paths_array){
      // cout << path+arg << endl;
      if(filesystem::exists(path+"/"+arg)){
        // cout << path+arg << endl;
        path_exists = true;
        filesystem::file_status s = filesystem::status(path+"/"+arg);
        filesystem::perms p = s.permissions();
        if((p & filesystem::perms::owner_exec) != filesystem::perms::none)
           {
            cout << arg << " is " << path+"/"+arg << "\n";
            
            break;
           }
      }
    }
    if(!path_exists){
      cout << arg << ": not found\n";
    }
  }
  
}

int main() {
  // Flush after every std::cout / std:cerr
  cout << unitbuf;
  cerr << unitbuf;
  string command;
  while(true){
    cout << "$ ";
    getline(cin,command);

    //parsing input line into an array
    vector<string> array;
    string temp;
    int size = command.size();
    
    for(int i=0; i<size; i++){
      if(command[i] == ' ' && temp != ""){
        array.push_back(temp);
        temp = "";
      }
      else{
        temp += command[i];
      }
    }
    if(temp != "")array.push_back(temp);

    //checking for valid commands and executing
    if(array[0] == "echo"){
      array.erase(array.begin());
      print_v(array);
      continue;
    }
    else if(array[0] == "type"){
      array.erase(array.begin());
      type_exec(array); 
    }
    else if(array[0] == "exit"){
      break;
    }
    else{
      cout << array[0] << ": command not found\n";
    }
  }
}
