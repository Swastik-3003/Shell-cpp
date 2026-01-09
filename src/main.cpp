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


int main() {
  // Flush after every std::cout / std:cerr
  cout << unitbuf;
  cerr << unitbuf;
  string command;
  while(true){
    cout << "$ ";
    getline(cin,command);
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
     
    if(array[0] == "echo"){
      array.erase(array.begin());
      print_v(array);
      continue;
    }
    else if(array[0] == "exit"){
      break;
    }
    else{
      cout << command << ": command not found\n";
    }
  }
}
