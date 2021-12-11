#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <unistd.h>
using namespace std;

typedef enum { STATE_CREATED, STATE_READY, STATE_RUNNING , STATE_BLOCKED} process_state_t;
typedef enum { TRANS_TO_READY, TRANS_TO_RUN, TRANS_TO_BLOCK, TRANS_TO_PREEMPT, TRANS_TO_DONE} event_trans_t;
static vector<int> randvals;
static int ofs;

template<typename T>
T pop_front(vector<T>& vec)
{
    T p = vec.front();
    vec.erase(vec.begin());
    return p;
}

int myrandom(int burst) { 
    return 1 + (randvals[ofs] % burst); 
    }

class Process{
    public:
    int at, tc, cb, io;
    int pid;
    int rt, st;
    int prio;
    process_state_t state;

    int ft, tt, it, cw;

    Process(){}

    Process(int at_, int tc_, int cb_, int io_, int pid_, int prio_ = 0){
        at = at_;
        tc = tc_;
        cb = cb_;
        io = io_;
        state = STATE_CREATED;
        pid = pid_;
        rt = 0;
        st = -1;
        prio = prio_;

        ft = -1;
        tt = -1;
        it = -1;
        cw = -1;
    }
};

ostream &operator<<(ostream &os, Process *p){
    os<<setw(4)<<setfill('0')<<p->pid<<":\t"<<p->at<<"\t"<<p->tc<<"\t"<<p->cb<<"\t"<<p->io<<"\t"<<p->prio
    <<"\t|\t"<<p->ft<<"\t"<<p->tt<<"\t"<<p->it<<"\t"<<p->cw<<endl;
    return os;
}

class Event{
    public:
    int timestamp;
    Process *p;
    event_trans_t trans;
    int oldstate;
    int newstate;

    Event(){}
    
    Event(int timestamp_, Process *p_, event_trans_t trans_){
        timestamp = timestamp_;
        p = p_;
        trans = trans_;
    }

    bool operator==(Event *e){
        if (e->p->pid == p->pid) return true;
        else return false;
    }

};

class Simulation{
    public:
    int time;
    vector<Event *> eventQ;

    Event* get_event(){
        if (!eventQ.empty()) return pop_front(eventQ);
        else return nullptr;
    }

    void put_event(Event *e){
        int i;
        for (i = 0; i < eventQ.size(); i++){
            if (eventQ[i]->timestamp > e->timestamp){
                break;
            }
        }
        eventQ.insert(eventQ.begin()+i, e);
    }

    void rm_event(Event *e){
        eventQ.erase(remove(eventQ.begin(), eventQ.end(), e), eventQ.end());
    }

    int get_next_event_time(){
        return eventQ[0]->timestamp;
    }

};

class Scheduler{
    public:
    vector<Process *> runQ;

    Scheduler(){}

    void add_process(Process* p){
        runQ.push_back(p);
    }

    Process* get_next_process(){
        if (!runQ.empty()) return pop_front(runQ);
        else return nullptr;
    }
};

int main(int argc, char** argv) {
    int opt;
    string input = "";
    string rfile = "";
    char *test = NULL;
    bool flagV = false;
    bool flagT = false;
    bool flagE = false;
    bool flagP = false;
    bool flagS = false;

    // Retrieve the (non-option) argument:
    if ( (argc <= 2) || (argv[argc-1] == NULL) || (argv[argc-1][0] == '-') ) {  // there is NO input...
        cerr << "No argument provided!" << endl;
        //return 1;
    }
    else {  // there is an input...
        input = argv[argc-2];
        rfile = argv[argc-1];
    }

    // Debug:
    cout << "input = " << input << endl;
    cout << "rfile = " << rfile << endl;

    // Shut GetOpt error messages down (return '?'): 
    opterr = 0;

    // Retrieve the options:
    while ( (opt = getopt(argc, argv, "vteps:")) != -1 ) {  // for each option...
        switch ( opt ) {
            case 'v':
                    flagV = true;
                break;
            case 't':
                    flagT = true;
                break;
            case 'e':
                    flagE = true;
                break;
            case 'p':
                    flagP = true;
                break;
            case 's':
                    flagS = true;
                    test = optarg;
                break;
            case '?':  // unknown option...
                    if (optopt == 's')
                    fprintf (stderr, "Option -%c requires an argument.\n", optopt);
                    else if (isprint (optopt))
                    fprintf (stderr, "Unknown option `-%c'.\n", optopt);
                    else
                    fprintf (stderr,
                            "Unknown option character `\\x%x'.\n",
                            optopt);
                break;
        }
    }

    cout << test << endl;

    ifstream rp;
    rp.open(rfile);
    string word;
    int ofs = 0;

    while (rp >> word){
        randvals.push_back(stoi(word));
    }

    ifstream fp;
    fp.open(input);
    int count = 0;
    int pidcount = 0;
    int words[4];


    Scheduler s = Scheduler();
    Simulation sim = Simulation();
    vector<Process *> processes;

    while (fp >> word){
        words[count] = stoi(word);
        count++;

        if (count == 4){
            count = 0;
            Process *p = new Process(words[0], words[1], words[2], words[3], pidcount);
            processes.push_back(p);
            Event *e = new Event(words[0], p, TRANS_TO_READY);
            sim.put_event(e);
            pidcount++;
        }
    }

    int CURRENT_TIME;
    bool CALL_SCHEDULER;
    int timeInPrevState;
    Process* CURRENT_RUNNING_PROCESS = nullptr;
    Process* CURRENT_BLOCKED_PROCESS = nullptr;
    Event *e;
    int burst, iotime;

    Event *evt;

    while(evt = sim.get_event()) {
        if (!evt){
            break;
        }
        for (auto i = sim.eventQ.begin(); i != sim.eventQ.end(); i++){
            cout<<(*i)->timestamp<<endl;
        }
        Process *proc = evt->p;
    // this is the process the event works on
        CURRENT_TIME = evt->timestamp;
        cout<<"CT : "<<CURRENT_TIME<<endl;
        // timeInPrevState = CURRENT_TIME – proc->state;
        switch(evt->trans) {  // which state to transition to?
        case TRANS_TO_READY:
            // must come from BLOCKED or from PREEMPTION
            // must add to run queue
            CURRENT_BLOCKED_PROCESS = proc;
            s.add_process(CURRENT_BLOCKED_PROCESS);
            CALL_SCHEDULER = true; // conditional on whether something is run
            break;
        case TRANS_TO_RUN:
            // create event for either preemption or blocking
            CURRENT_RUNNING_PROCESS = proc;
            if (CURRENT_RUNNING_PROCESS->rt > 0){
                burst = proc->rt;
            }
            else{
                burst = myrandom(proc->cb);
                ofs++;
            }
            if (CURRENT_RUNNING_PROCESS->st == -1){
                CURRENT_RUNNING_PROCESS->st = CURRENT_TIME;
            }
            if (burst > CURRENT_RUNNING_PROCESS->tc){
                cout<<"Here"<<endl;
                e = new Event(CURRENT_TIME+burst, CURRENT_RUNNING_PROCESS, TRANS_TO_DONE);
                sim.put_event(e);
                
            }
            else {
                e = new Event(CURRENT_TIME+burst, CURRENT_RUNNING_PROCESS, TRANS_TO_BLOCK);
                sim.put_event(e);
                CURRENT_RUNNING_PROCESS->rt = -1;
                CURRENT_RUNNING_PROCESS->tc -= burst;
            }
            break;
        case TRANS_TO_BLOCK:
            //create an event for when process becomes READY again
            CURRENT_BLOCKED_PROCESS = proc;
            iotime = myrandom(CURRENT_BLOCKED_PROCESS->io);
            ofs++;
            e = new Event(CURRENT_TIME+iotime, CURRENT_BLOCKED_PROCESS, TRANS_TO_READY);
            sim.put_event(e);
            CALL_SCHEDULER = true;
            break;
        case TRANS_TO_PREEMPT:
            // add to runqueue (no event is generated)

            CALL_SCHEDULER = true;
            break;
        case TRANS_TO_DONE:
            proc->ft = CURRENT_TIME;
            CURRENT_RUNNING_PROCESS = nullptr;
            break;
        }
        // remove current event object from Memory

        // delete evt; evt = nullptr;

        if(CALL_SCHEDULER) {

            if (sim.get_next_event_time() == CURRENT_TIME)
                continue; //process next event from Event queue
            CALL_SCHEDULER = false; // reset global flag
            if (CURRENT_RUNNING_PROCESS == nullptr) {

                CURRENT_RUNNING_PROCESS = s.get_next_process();

                if (CURRENT_RUNNING_PROCESS == nullptr)
                    continue;
                
                e = new Event(CURRENT_TIME, CURRENT_RUNNING_PROCESS, TRANS_TO_RUN);
                sim.put_event(e);

                CURRENT_RUNNING_PROCESS = nullptr;
            }
        }
    }

    for (auto i = processes.begin() ; i != processes.end() ; i++){
        cout<<(*i);
    }

    return 0;
}