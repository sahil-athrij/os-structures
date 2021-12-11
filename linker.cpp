#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <regex>
#include <utility>
#include <iomanip>

using namespace std;
#define MACHINESIZE 512
#define WORDSIZE 16
#define DEFSIZE 16
#define USESIZE 16
#define INSSIZE 9999

void ReplaceStringInPlace(string& subject, const string& search,
                          const string& replace) {
    size_t pos = 0;
    while((pos = subject.find(search, pos)) != string::npos) {
         subject.replace(pos, search.length(), replace);
         pos += replace.length();
    }
}

template <typename T>
bool contains(vector<T> vec, const T & elem)
{
    bool result = false;
    if( find(vec.begin(), vec.end(), elem) != vec.end() )
    {
        result = true;
    }
    return result;
}

int findOffset(string filename, string s, int &count) {
	int i = 0;
	string line;
    string word;
    ifstream fp;
	fp.open(filename);
	if (s==""){
		string prevword;
		string prevline;
		int offset;

		while (getline(fp,line)){
			if ((line.length() > 0) && (line.at(0) != ' ') && (line.at(0)!= '\n') && (line.at(0)!='\0' )){
				prevline = line;
				i++;
			}
            else if(line.length() == 0){
                prevline = line;
                i++;
            }
            else break;
		}
		i--;
        count = i;
		return prevline.length();
	}
	while (getline(fp,line)){
		if (i == count){
			int offset = line.find(s);
			if (offset != -1)
				return offset;
			else {
				getline(fp, line);
				count++;
				return line.find(s);
			}
		}
		i++;
	}
	fp.close();
	return -1;
}

void countLine(ifstream& fp, int &count, int &linestart, int &prevline, int &wordstart, int &prevwordstart){
    prevwordstart = wordstart;
    if (fp.peek() == '\n'){
        prevline = linestart;
        count++;
        linestart = fp.tellg();
    }
    wordstart = fp.tellg();
}

bool isNumber(string word){
    for(auto i = 0; i < word.size(); i++){
        if(!isdigit(word[i])) return false;
    }
    return true;
}

class Error{
    public:
    int type;
    string msg;
    vector<string> msgs{
		"NUM_EXPECTED",		// 0 number expected
		"SYM_EXPECTED",		// 1 symbol expected
		"ADDR_EXPECTED", 	// 2 addressing expected
		"SYM_TOLONG", 		// 3 symbol name is too long
		"TOO_MANY_DEF_IN_MODULE",	// 4 >16
		"TOO_MANY_USE_IN_MODULE",	// 5 >16
		"TOO_MANY_INSTR",	// 6 total num_instr exceeds 512
        "Error: Absolute address exceeds machine size; zero used", //7
        "Error: Relative address exceeds module size; zero used", //8
        "Error: External address exceeds length of uselist; treated as immediate", //9
        "Error: {name} is not defined; zero used", //10
        "Error: This variable is multiple times defined; first value used", //11
        "Error: Illegal immediate value; treated as 9999", //12
        "Error: Illegal opcode; treated as 9999", //13
        "Warning: Module {mod}: {name} too big {adr} (max={max}) assume zero relative", //14
        "Warning: Module {mod}: {name} appeared in the uselist but was not actually used", //15
        "Warning: Module {mod}: {name} was defined but never used", //16
    };

    Error(){}
    
    Error(int type_, string name = "", int modno = 0, int modsize = 0, int reladr = 0, bool flag = false){
        type = type_;
        msg = msgs[type];
        if(type < 7){
            cout<<"Parse Error line "<<modno+1<<" offset "<<modsize+1<<": "<<msg<<endl;
            exit(1);
        }
        if(modno){
            ReplaceStringInPlace(msg, "{mod}", to_string(modno));
        }
        if(name != ""){
            ReplaceStringInPlace(msg, "{name}", name);
        }
        if(modsize){
            ReplaceStringInPlace(msg, "{max}", to_string(modsize));
        }
        if(reladr || flag){
            ReplaceStringInPlace(msg, "{adr}", to_string(reladr));
        }
    }
};

class Variable{
    public:
    string name;
    int reladr;
    int absadr;
    int modno;
    Error err;
    bool errflag = false;
    bool created = false;
    bool used = false;
    int count = 0;

    Variable(){}

    Variable(string name_, string reladr_, int modno_ = 0, int baseadr_ = 0){
        name = name_;
        reladr = stoi(reladr_);
        modno = modno_;
        absadr = baseadr_+reladr;
        created = true;
    }

};

ostream &operator<<(ostream &os, const Variable &v){
    os<<v.name<<"="<<v.absadr;
    if(v.errflag){
        os<<" "<<v.err.msg;
    }
    return os;
}

class Instruction{
    public:
    string type;
    int opcode;
    int operand;
    int baseadr;
    bool errflag = false;
    Error err;

    Instruction(string type_, string instruction_, int modno_ = 0, int baseadr_ = 0){
        type = type_;
        baseadr = baseadr_;
        int instruction = stoi(instruction_);
        opcode = instruction/1000;
        operand = instruction%1000;
    }

    Instruction(string type_, string instruction_, int modno_, int baseadr_, map<string, Variable> symtab, vector< pair <string,int> > &uselist, int size = 1, vector<string> *vlist = NULL){
        type = type_;
        baseadr = baseadr_;
        int instruction = stoi(instruction_);
        opcode = instruction/1000;
        operand = instruction%1000;
        if(type == "I"){
            if(instruction > INSSIZE){
                err = Error(12);
                errflag = true;
                opcode = 9;
                operand = 999;
            }
        }
        else if(opcode > 9){
            opcode = 9;
            operand = 999;
            err = Error(13);
            errflag = true;
        }
        else{
            if(type == "R"){
                if(operand > size){
                    operand = baseadr;
                    err = Error(8);
                    errflag = true;
                }
                else{
                    operand += baseadr;
                }
            }
            
            if(type == "E"){
                if(uselist.size() <= operand){
                    err = Error(9);
                    errflag = true;
                }
                else{
                    string sym = uselist[operand].first;
                    if(symtab[sym].created){
                        if(vlist){
                            // cout<<"Pushing"<<endl;
                            vlist->push_back(sym);
                        }
                        uselist[operand].second++;
                        operand = symtab[sym].absadr;
                    }
                    else{
                        uselist[operand].second++;
                        err = Error(10, sym);
                        errflag = true;
                        operand = 0;
                    }
                }
            }

            if(type == "A"){
                if(operand >= MACHINESIZE){
                    err = Error(7);
                    errflag = true;
                    operand = 0;
                }
                
            }
        }
    }

};

ostream &operator<<(ostream &os, const Instruction &ins){
    os<<setw(4)<<setfill('0')<<ins.opcode*1000 + ins.operand;
    if(ins.errflag){
        os<<" "<<ins.err.msg;
    }
    return os;
}

class Module{
    public:
    int baseadr;
    int modno;
    int modsize;
    int endadr;
    vector<Variable> deflist;
    vector< pair <string,int> > uselist;
    vector<Instruction> inslist;
    vector<Error> warnlist;
    vector<Variable> alldeflist;

    Module(){}

    Module(int baseadr_, int modno_){
        baseadr = baseadr_;
        modno = modno_;
    }

    void makeDefList(ifstream& fp, int defcount, map<string, Variable> &symtab, int &linecount, int &linestart, int &prevline, int &wordstart, int &prevwordstart, string filename, bool pass1 = true){
        string word, prevword;

        for(auto i=0; i<2*defcount; i++){
            countLine(fp, linecount, linestart, prevline, wordstart, prevwordstart);
            if(fp>>word){
                if(i%2 == 0){
                    if (!isalpha(word.at(0))) {
                        int offset = findOffset(filename, word, linecount);
                        Error err = Error(1, "", linecount, offset);
                    }
                    if (word.length() > WORDSIZE) {
                        int offset = findOffset(filename, word, linecount);
                        Error err = Error(3, "", linecount, offset);
                    }
                }
                else{
                    if(!isNumber(word)){
                        int offset = findOffset(filename, word, linecount);
                        Error err = Error(0, "", linecount, offset);
                    }
                    else{
                        Variable v{prevword, word, modno, baseadr};
                        alldeflist.push_back(v);
                        if(!pass1 && symtab.count(prevword) == 1 && !symtab[prevword].used){
                            bool flag = true;
                            for(auto i = deflist.begin(); i != deflist.end(); i++){
                                if((*i).name == prevword){
                                    flag = false;
                                    break;
                                }
                            }
                            if(flag){
                                deflist.push_back(v);
                                symtab[prevword].used = true;
                            }
                        }
                        if(symtab.count(prevword) > 0){
                            symtab[prevword].err = Error(11);
                            symtab[prevword].errflag = true;
                        }
                        else{
                            if(pass1){
                                deflist.push_back(v); 
                            }
                            symtab.insert(pair<string, Variable> (v.name, v));
                        }
                    }
                }
                prevword = word;
            }
            else{
                if (i%2 == 0) {
                    int offset = findOffset(filename, "", linecount);
                    Error err = Error(1, "", linecount, offset);
                }
                else {
                    int offset = findOffset(filename, "", linecount);
                    Error err = Error(0, "", linecount, offset);
                }
            }
        }
    }

    void updateDefList(map<string, Variable> &symtab){
        int t = modsize;
        for(auto i = deflist.begin(); i != deflist.end(); i++){
            if(modsize < (*i).reladr){
                string name = (*i).name;
                symtab[name].reladr = 0;
                symtab[name].absadr = baseadr;
                (*i).reladr = 0;
                (*i).absadr = baseadr;
            }
            else{
            }
        }
        for(auto i = alldeflist.begin(); i != alldeflist.end(); i++){
            string name = (*i).name;
            if(modsize < (*i).reladr){
                if(symtab[name].count > 0){
                    Error err = Error(14, name, modno+1, modsize-1, 0, true);
                    warnlist.push_back(err);
                }
                else{
                    Error err = Error(14, name, modno+1, modsize-1, (*i).reladr);
                    warnlist.push_back(err);
                }
                (*i).reladr = 0;
                (*i).absadr = baseadr;
            }
            else{
            }
            symtab[name].count++;
        }
    }

    void makeUseList(ifstream& fp, int usecount, int &linecount, int &linestart, int &prevline, int &wordstart, int &prevwordstart , string filename){
        string word;

        for(auto i=0; i<usecount; i++){
            countLine(fp, linecount, linestart, prevline, wordstart, prevwordstart);
            if(fp>>word){
                if (!isalpha(word.at(0))) {
                    int offset = findOffset(filename, word, linecount);
                    Error err = Error(1, "", linecount, offset);
                }
                uselist.push_back(pair<string, int> (word, 0));
            }
            else{
                int offset = findOffset(filename, "", linecount);
                Error err = Error(1, "", linecount, offset);
            }
        }
    }

    void makeInsList(ifstream& fp, int inscount, int &linecount, int &linestart, int &prevline, int &wordstart, int &prevwordstart, string filename){
        string word, prevword;
        if (fp.peek() == EOF && inscount>1) {
            int offset = findOffset(filename, "", linecount);
            Error err = Error(2, "", linecount, offset);
        }
        for(auto i=0; i<2*inscount; i++){
            countLine(fp, linecount, linestart, prevline, wordstart, prevwordstart);
            if(fp>>word){
                if(i%2 == 0){
                    if(!(word=="I" || word=="R" || word== "A" || word=="E")){
                        int offset = findOffset(filename, word, linecount);
                        Error err = Error(2, "", linecount, offset);
                    }
                }
                else{
                    if(isNumber(word)){
                        Instruction ins{prevword, word, modno, baseadr};
                        inslist.push_back(ins);
                    }
                    else{
                        int offset = findOffset(filename, word, linecount);
                        Error err = Error(2, "", linecount, offset);
                    }
                }
                prevword = word;
            }
            else{
                if(i%2 == 0){
                    int offset = findOffset(filename, "", linecount);
                    Error err = Error(2, "", linecount, offset);
                }
                else{
                    int offset = findOffset(filename, "", linecount);
                    Error err = Error(1, "", linecount, offset);
                }
            }
        }
        modsize = inscount;
        // cout<<"modsize ins1 : "<<modsize<<endl;
    }

    void makeInsListWithSymTab(ifstream& fp, int inscount, map<string, Variable> symtab, vector<string> &vlist){
        string word, prevword;
        int index;
        for(auto i=0; i<2*inscount; i++){
            fp>>word;

            if(i%2 == 0){
                // TODO: Check if it complies with instruction
            }
            else{
                Instruction ins{prevword, word, modno, baseadr, symtab, uselist, inscount, &vlist};
                inslist.push_back(ins);
            }
            prevword = word;
        }
        modsize = inscount;
        // cout<<"modsize ins2 : "<<modsize<<endl;
    }

    void printInstruction(){
        int counter = 0;
        for(auto i = inslist.begin(); i != inslist.end(); i++){
            cout<<setw(3)<<setfill('0')<<baseadr+counter<<": "<<(*i)<<endl;
            counter++;
        }
    }

    void findInterWarnList(){
        for(auto i = uselist.begin(); i != uselist.end(); i++){
            if((*i).second == 0){
                Error err = Error(15, (*i).first, modno+1);
                warnlist.push_back(err);
            }
        }
    }

    void findWarnList(vector<string> vlist){
        for(auto i = deflist.begin(); i != deflist.end(); i++){
            string name = (*i).name;
            if(!contains(vlist,name)){
                Error err = Error(16, name, modno+1);
                warnlist.push_back(err);
            }
        }
    }
};

ostream &operator<<(ostream &os, const Module &mod){
    for(auto i = mod.warnlist.begin(); i != mod.warnlist.end(); i++){
        os<<(*i).msg<<endl;
    }
    return os;
}

map<string, Variable> pass1(char* filename){
    ifstream fp;
    string word;
    int modcount = 0;
    int baseadr = 0;
    int linecount = 0;
    int prevline = 0;
    int linestart = 0;
    int wordstart, prevwordstart = 0;
    vector<Module> modules;
    map<string, Variable> symtab;

    fp.open(filename);
    countLine(fp, linecount, linestart, prevline, wordstart, prevwordstart);
    while(fp>>word){
        Module mod{baseadr, modcount};

        if(!isNumber(word)){
            int offset = findOffset(filename, "", linecount);
            Error err = Error(0, "", linecount, offset);
        }
        else{
            int defcount = stoi(word);
            if(defcount > DEFSIZE){
                int offset = findOffset(filename, word, linecount);
                Error err = Error(4, "", linecount, offset);
            }
            else{
                mod.makeDefList(fp, defcount, symtab, linecount, linestart, prevline, wordstart, prevwordstart, filename);
            }
        }
        countLine(fp, linecount, linestart, prevline, wordstart, prevwordstart);
        fp>>word;
        if(!isNumber(word)){
            int offset = findOffset(filename, "", linecount);
            Error err = Error(0, "", linecount, offset);
        }
        else{
            int usecount = stoi(word);
            if(usecount > USESIZE){
                int offset = findOffset(filename, word, linecount);
                Error err = Error(5, "", linecount, offset);
            }
            mod.makeUseList(fp, usecount, linecount, linestart, prevline, wordstart, prevwordstart, filename);
        }
        countLine(fp, linecount, linestart, prevline, wordstart, prevwordstart);
        fp>>word;
        if(!isNumber(word)){
            int offset = findOffset(filename, "", linecount);
            Error err = Error(0, "", linecount, offset);
        }
        else{
            int inscount = stoi(word);
            if(inscount + baseadr > MACHINESIZE){
                int offset = findOffset(filename, word, ++linecount);
                Error err = Error(6, "", linecount, offset);
            }
            mod.makeInsList(fp, inscount, linecount, linestart, prevline, wordstart, prevwordstart, filename);
        }
        // cout<<"before base : "<<mod.modsize<<endl;
        mod.updateDefList(symtab);
        cout<<mod;
        mod.warnlist.clear();
        modules.push_back(mod);
        modcount++;
        baseadr += mod.modsize;
        // cout<<"baseadr mod1 : "<<mod.modsize<<endl;
    }
    cout<<"Symbol Table"<<endl;
    for (auto i = modules.begin(); i != modules.end(); i++){
        for(auto j = (*i).deflist.begin(); j != (*i).deflist.end(); j++){
            cout<<symtab[(*j).name]<<endl;
        }
    }
    cout<<endl;

    fp.close();

    return symtab;
}

void pass2(char* filename, map<string, Variable> symtab){
    ifstream fp;
    string word;
    int modcount = 0;
    int baseadr = 0;
    int linecount = 0;
    int prevline = 0;
    int linestart = 0;
    int wordstart, prevwordstart = 0;
    vector<Module> modules;
    vector<string> vlist;
    fp.open(filename);
    countLine(fp, linecount, linestart, prevline, wordstart, prevwordstart);
    while(fp>>word){
        Module mod{baseadr, modcount};

        if(!isNumber(word)){
            // TODO: Error for not number in starting
        }
        else{
            int defcount = stoi(word);
            mod.makeDefList(fp, defcount, symtab, linecount, linestart, prevline, wordstart, prevwordstart, filename, false);
        }
        countLine(fp, linecount, linestart, prevline, wordstart, prevwordstart);
        fp>>word;
        if(!isNumber(word)){
            // TODO: Error for not number in starting
        }
        else{
            int usecount = stoi(word);
            mod.makeUseList(fp, usecount, linecount, linestart, prevline, wordstart, prevwordstart , filename);
        }
        countLine(fp, linecount, linestart, prevline, wordstart, prevwordstart);
        fp>>word;
        if(!isNumber(word)){
            // TODO: Error for not number in starting
        }
        else{
            int inscount = stoi(word);

            mod.makeInsListWithSymTab(fp, inscount, symtab, vlist);
        }

        modules.push_back(mod);
        modcount++;
        baseadr += mod.modsize;
        // cout<<"baseadr mod2 : "<<mod.modsize<<endl;
    }
    cout<<"Memory Map"<<endl;
    for(auto i = modules.begin(); i != modules.end(); i++){
        (*i).printInstruction();
        (*i).findInterWarnList();
        cout<<(*i);
        (*i).warnlist.clear();
    }
    cout<<endl;
    // for(auto i = vlist.begin(); i != vlist.end(); i++){
    //     cout<<"vlist : "<<(*i)<<endl;
    // }
    for(auto i = modules.begin(); i != modules.end(); i++){
        (*i).findWarnList(vlist);
        cout<<(*i);
    }
    cout<<endl;
}

int main(int argc, char* argv[]){
    char* filename = argv[1];
    auto symtab = pass1(filename);
    pass2(filename, symtab);
    return 0;
}