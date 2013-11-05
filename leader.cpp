//logging
#include <log4cxx/logger.h>
#include <log4cxx/xml/domconfigurator.h>

#include<stdio.h>
#include <pthread.h>
#include<algorithm>
#include "pax_types.h"

using namespace log4cxx;
using namespace log4cxx::xml;
using namespace log4cxx::helpers;

// Define static logger variable
LoggerPtr LeaderLogger(Logger::getLogger("leader"));

#define MAX_SET_SIZE 100
#define TALKER leader_comm.comm_fd[TALKER_INDEX]
#define LISTENER leader_comm.comm_fd[LISTENER_INDEX]
int ACCEPTOR_PORT_LIST[MAX_ACCEPTORS] = {3000,3002,3004};//,3006,3008,3010,3012,3014,3016,3018};
int LEADER_PORT_LIST[MAX_LEADERS] = {4000,4002};
int REPLICA_PORT_LIST[MAX_REPLICAS] = {2000,2002};
int COMMANDER_PORT_LIST[MAX_COMMANDERS] = {5000,5001,5002,5003,5004,5005,5006,5007,5008,5009,5010,5011,5012,5013,5014,5015,5016,5017,5018,5019,5020,5021,5022,5023,5024,5025,5026,5027,5028,5029,5030,5031,5032,5033,5034,5035,5036,5037,5038,5039,5040,5041,5042,5043,5044,5045,5046,5047,5048,5049,5050,5051,5052,5053,5054,5055,5056,5057,5058,5059};
int SCOUT_PORT_LIST[MAX_SCOUTS] = {6000,6001,6002,6003,6004,6005,6006,6007,6008,6009,6010,6011,6012,6013,6014,6015,6016,6017,6018,6019,6020,6021,6022,6023,6024,6025,6026,6027,6028,6029,6030,6031,6032,6033,6034,6035,6036,6037,6038,6039,6040,6041,6042,6043,6044,6045,6046,6047,6048,6049,6060,6051,6052,6053,6054,6055,6056,6057,6058,6059};

void* commander(void*);
void* scout(void*);


//create a mapping such that acc_pvals[slot_number][0/1] = (command/highest ballot number) with highest ballot 
#define POST_PROCESS_PVALS(ACC_MAP1,ACC_MAP2,STR) \
					 temp = strtok_r(STR,DELIMITER_SEC,&tok1); \
					while(temp) \
					{ \
						printf("here %s\n",temp);\
						/*BALLOT NUMBER (temp1,temp2)*/ \
						recv_ballot.bnum = atoi(temp); \
						recv_ballot.leader_id = atoi(strtok_r(NULL,DELIMITER_SEC,&tok1)); \
						/*slot number */ \
						slot_number = atoi(strtok_r(NULL,DELIMITER_SEC,&tok1)); \
						command = atoi(strtok_r(NULL,DELIMITER_SEC,&tok1)); \
						if(ballot_compare(recv_ballot,acc_pvals_hballot[slot_number]) > 0) \
						{ \
							printf("in ballot compare \n");\
							ACC_MAP1[slot_number] = command; \
							ACC_MAP2[slot_number] = recv_ballot; \
						} \
						temp = strtok_r(NULL,DELIMITER_SEC,&tok1); \
					}


				//manipulate the leaders pending proposals according the acc_pvals_map
#define MANIPULATE_LEADER_PLIST(ACC_CMD_MAP,PROP_LIST) \
					for(i=0;i<MAX_SLOTS;i++) \
					{ \
						if(ACC_CMD_MAP[i] != -1) \
						PROP_LIST.command[i] = ACC_CMD_MAP[i]; \
					} 

enum LEADER_STATUS
{
	LEADER_INACTIVE=0,	
	LEADER_ACTIVE	
};
struct STATE_LEADER
{
	struct BALLOT_NUMBER ballot;
	enum LEADER_STATUS lstatus;
	struct PROPOSAL plist;
};

int ballot_compare(struct BALLOT_NUMBER other_ballot,struct BALLOT_NUMBER my_ballot)
{
	if(other_ballot.bnum-my_ballot.bnum == 0)
	{
		return (other_ballot.leader_id-my_ballot.leader_id);	
	}
	else
	{
		return(other_ballot.bnum-my_ballot.bnum); 
	}
}
void ballot_copy(struct BALLOT_NUMBER other_ballot,struct BALLOT_NUMBER *my_ballot)
{
	my_ballot->bnum = other_ballot.bnum;
	my_ballot->leader_id = other_ballot.leader_id;
}

bool configure_leader(int my_pid,struct COMM_DATA *comm_leader)
{
	
	int listener_fd,port,talker_fd;
	socklen_t listener_len;
	int yes=1;
	int i;
	struct sockaddr_in talker_addr, listener_addr, *process_addr_in;
	//listen setup
	if ((listener_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
		perror("listener socket ");
        	return false;
    	}

	if (setsockopt(listener_fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1) {
		perror("setsockopt");
		return false;
	}
    	listener_addr.sin_family = AF_INET;
   	listener_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	listener_addr.sin_port = htons(LEADER_PORT_LIST[my_pid]); 

	if (bind(listener_fd, (struct sockaddr *) &listener_addr, sizeof(listener_addr)) < 0)
	{
	        perror("listener bind ");
	        close(listener_fd);
	        return false;
	}
    	listener_len = sizeof(listener_addr);

	if (getsockname(listener_fd, (struct sockaddr *)&listener_addr, &listener_len) < 0)
    	{
        	perror("listener getsockname ");
        	close(listener_fd);
        	return false;
    	}
    	printf("leader: listener using port %d\n", ntohs(listener_addr.sin_port));

	//talker setup
	if (( talker_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
			perror("talker socket ");
       			return false;
	}
	printf("leader: talker_fd = %d\n",talker_fd);

	talker_addr.sin_family = AF_INET;
	talker_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    	talker_addr.sin_port = htons(0);  // pick any free port

	if (bind(talker_fd, (struct sockaddr *) &talker_addr, sizeof(talker_addr)) < 0)
    	{
        perror("leader: talker bind ");
        close(talker_fd);
        return false;
    	}   

	comm_leader->comm_fd[LISTENER_INDEX] = listener_fd;
	comm_leader->comm_fd[TALKER_INDEX] = talker_fd;

return true;
}

int main(int argc,char **argv)
{
	struct COMM_DATA leader_comm;
	struct STATE_LEADER leader_state;
	int my_pid;

//comm common
	struct sockaddr_storage temp_paddr;
	socklen_t temp_paddr_len;

//comm related variables
	struct hostent *hp;
	char hostname[64];

//comm listening variables
	fd_set readfds;
	int maxfd;
	char recv_buff[BUFSIZE];
	int nread=0;	

//misc
	int i,ret=0,recv_pid;
	bool command_found = false;
	char *data,*temp,*tok,*tok1;
	char *ballot_str,*pvals_str,*pstr;
	char buff_copy[BUFSIZE];
	int slot_number,command;
	struct BALLOT_NUMBER recv_ballot,temp_ballot;

//threads
	pthread_t commander_thread[MAX_COMMANDERS_PER_LEADER],scout_thread[MAX_SCOUTS_PER_LEADER];
	int rc=0;
	struct COMMANDER_THREAD_ARG comm_create_args[MAX_COMMANDERS_PER_LEADER];
	int count_commanders=0, count_scouts=0;
	struct SCOUT_THREAD_ARG scout_create_args[MAX_SCOUTS_PER_LEADER];

//pval set from acceptors
	struct BALLOT_NUMBER acc_pvals_hballot[MAX_SLOTS]; //// this contains acc_pvals_hballot[slot_number]= current highest ballot
	int acc_pvals_command[MAX_SLOTS] = {-1}; //for time being this is int to into - might changed based on type of 'command'
					// this contains acc_pvals_command[slot_number]= command with highest ballot number from acceptor 

	//check runtime arguments
	if(argc!=2)
	{
		printf("Usage: ./leader <leader_id>\n");
		return -1;
	}
	my_pid=atoi(argv[1]);
//configure logger
 DOMConfigurator::configure("leader_log_config.xml");
	//initialization
	//leader_state.plist.current_length = 0;
	leader_state.ballot.bnum = 0;
	leader_state.ballot.leader_id = my_pid;
	leader_state.lstatus = LEADER_INACTIVE;

	std::fill(acc_pvals_command, acc_pvals_command + MAX_SLOTS, -1);
	std::fill(leader_state.plist.command,leader_state.plist.command+MAX_SLOTS,-1);

	//hostname configuration
	gethostname(hostname, sizeof(hostname));
	hp = gethostbyname(hostname);
	if (hp == NULL) 
	{ 
		LOG4CXX_TRACE(LeaderLogger,hostname << " : unknown host\n");
		//printf("\n%s: unknown host.\n", hostname); 
		return 0; 
	} 
	//configure leader talker and listener ports	
	//setup the leader 
	if(configure_leader(my_pid,&leader_comm))
	{
		LOG4CXX_TRACE(LeaderLogger,"Leader id: " << my_pid << " configured successfully\n");
		//printf("Leader id: %d configured successfully\n",my_pid);
	}
	else
	{
		LOG4CXX_TRACE(LeaderLogger,"Leader id: " << my_pid << " config error \n");
		//printf("Error in config of leader id: %d\n",my_pid);
		return -1;
	}
	
#if DEBUG==1
	LOG4CXX_TRACE(LeaderLogger,"Leader id: " << my_pid << "creating scout thread for ballot (" << leader_state.ballot.bnum << "," << leader_state.ballot.leader_id <<  ") !\n");
	//printf("Leader id: %d creating scout thread for ballot (%d,%d)!\n",my_pid,leader_state.ballot.bnum,leader_state.ballot.leader_id);
#endif
	//create a new scout thread
	scout_create_args[count_scouts].parent_id = my_pid;
	scout_create_args[count_scouts].my_pid = count_scouts+my_pid*MAX_SCOUTS_PER_LEADER;
	scout_create_args[count_scouts].my_ballot = leader_state.ballot; 
	rc = pthread_create(&scout_thread[count_scouts], NULL, scout, (void *)&scout_create_args[count_scouts]);
	count_scouts++;

	while(1)
	{
		maxfd = LISTENER+1;
		FD_ZERO(&readfds); 
		FD_SET(LISTENER, &readfds);

		ret = select(maxfd, &readfds, NULL, NULL, NULL);  //blocks forever till it receives a message


		if(ret <0)
	   	{ 
			LOG4CXX_TRACE(LeaderLogger,"Leader id: " << my_pid << " select error\n");
	     		//printf("\nLeader id: %d Select error\n",my_pid);   
	     		return -1;
	   	} 

		if(FD_ISSET (LISTENER, &readfds))
		{
			temp_paddr_len = sizeof(temp_paddr);
			nread = recvfrom (LISTENER, recv_buff, BUFSIZE, 0, 
               	       			(struct sockaddr *)&temp_paddr, &temp_paddr_len); 
		
		 	if (nread < 0)
		       	{
		        	perror("recvfrom ");
            			close(LISTENER);
            			return -1;
        		}		
			recv_buff[nread] = 0;
  			//printf("Leader id: %d received: %s\n",my_pid, recv_buff);

			strcpy(buff_copy,recv_buff);			
			data = strtok_r(buff_copy,DELIMITER,&tok);

//retrive recv_pid
				recv_pid = atoi(strtok_r(NULL,DELIMITER,&tok));
#if DEBUG==1
				LOG4CXX_TRACE(LeaderLogger,"Leader id: " << my_pid << " received: " << recv_buff << "from: " << recv_pid << "\n");
				//printf("Leader id: %d recved msg from %d\n",my_pid,recv_pid);
#endif		
			if(strcmp(data,"PROPOSE") == 0)
			{
				//received from REPLICA
				//expects data in the format
				//PROPOSE:REPLICA_ID:SLOTNUMBER:COMMAND:

				//retrive slot num
				slot_number = atoi(strtok_r(NULL,DELIMITER,&tok));
			
				//retrive command
				command = atoi(strtok_r(NULL,DELIMITER,&tok));
				
				if(leader_state.plist.command[slot_number] == -1)
				{
					//new proposal
#if DEBUG==1
					LOG4CXX_TRACE(LeaderLogger,"Leader id: " << my_pid << "no previous proposal for slot number -- added to plist!\n");
					//printf("Leader id: %d no previous proposal for slot number -- added to plist!\n",my_pid);
#endif

					//add new proposal to plist
					leader_state.plist.command[slot_number] = command;
					
					if(leader_state.lstatus == LEADER_ACTIVE)
					{
#if DEBUG==1
						LOG4CXX_TRACE(LeaderLogger,"Leader id: " << my_pid << "is active now - creating commander thread" << count_commanders+my_pid*MAX_COMMANDERS_PER_LEADER << "\n");
						//printf("Leader id: %d  is active now - creating commander thread %d!\n",my_pid,count_commanders);
#endif
						//create a new commander thread
						comm_create_args[count_commanders].parent_id = my_pid;
						comm_create_args[count_commanders].my_pid = count_commanders+my_pid*MAX_COMMANDERS_PER_LEADER;
						comm_create_args[count_commanders].my_pval.ballot = leader_state.ballot; 
						comm_create_args[count_commanders].my_pval.slot_number = slot_number;
						comm_create_args[count_commanders].my_pval.command = command;
						rc = pthread_create(&commander_thread[count_commanders], NULL, commander, (void *)&comm_create_args[count_commanders]);
#if DEBUG==1
						LOG4CXX_TRACE(LeaderLogger,"Leader id: " << my_pid << "created commander thread" << count_commanders+my_pid*MAX_COMMANDERS_PER_LEADER << "\n");
						//printf("Leader id: %d created commander thread %d!\n",my_pid,count_commanders);
#endif
						count_commanders++;


					}
				}
				else
				{
					//old proposal
#if DEBUG==1
					LOG4CXX_TRACE(LeaderLogger,"Leader id: " << my_pid << "already proposed for this slot number\n");
					//printf("Leader id: %d already proposed for this slot number!\n",my_pid);
#endif
				}

			}
			else if(strcmp(data,"ADOPTED") == 0)
			{
				//received from SCOUT
				//expects data in the format
				//ADOPTED:SCOUT_ID:BALLOT:PVALS:

				ballot_str = strtok_r(NULL,DELIMITER,&tok);
				pvals_str = strtok_r(NULL,DELIMITER,&tok);

				recv_ballot.bnum = atoi(strtok_r(ballot_str,DELIMITER_SEC,&tok1));
				recv_ballot.leader_id = atoi(strtok_r(NULL,DELIMITER_SEC,&tok1));

				if(ballot_compare(recv_ballot,leader_state.ballot) == 0)
				{
					
					if(pvals_str)
					{
						// fetch PVALS and insert into PVAL struct
						//create a mapping such that acc_pvals[slot_number] = command with highest ballot 
						POST_PROCESS_PVALS(acc_pvals_command,acc_pvals_hballot,pvals_str);
						//manipulate the leaders pending proposals according the acc_pvals_map
						MANIPULATE_LEADER_PLIST(acc_pvals_command,leader_state.plist);
					}
					//create commander thread for each pending proposal with leader
					for(i=0;i<MAX_SLOTS;i++)
					{
	
						if(leader_state.plist.command[i] == -1)
							continue;

printf("!!!!here\n");
#if DEBUG==1
						LOG4CXX_TRACE(LeaderLogger,"Leader id: " << my_pid << " creating commander thread "  <<count_commanders+my_pid*MAX_COMMANDERS_PER_LEADER << " for proposal <(" << leader_state.ballot.bnum <<","<< leader_state.ballot.leader_id << "):" << i << ":"<< leader_state.plist.command[i] <<  ">!\n");
						//printf("Leader id: %d creating commander thread for proposal <(%d,%d):%d:%d>!\n",my_pid,leader_state.ballot.bnum,leader_state.ballot.leader_id,i,leader_state.plist.command[i]);
#endif
						//create a new commander thread
						comm_create_args[count_commanders].parent_id = my_pid;
						comm_create_args[count_commanders].my_pid = count_commanders+my_pid*MAX_COMMANDERS_PER_LEADER;
						comm_create_args[count_commanders].my_pval.ballot = leader_state.ballot; 
						comm_create_args[count_commanders].my_pval.slot_number = i;
						comm_create_args[count_commanders].my_pval.command = leader_state.plist.command[i];
						rc = pthread_create(&commander_thread[count_commanders], NULL, commander, (void *)&comm_create_args[count_commanders]);
						count_commanders++;

					}
					//change status to active
					leader_state.lstatus = LEADER_ACTIVE;
				}
				else
				{
					LOG4CXX_TRACE(LeaderLogger,"Leader id: " << my_pid << "received ballot ("<< recv_ballot.bnum << ":" << recv_ballot.leader_id << ")\n");
					//printf("Leader id: %d received ballot (%d:%d) which must be old - ignoring\n",my_pid,recv_ballot.bnum,recv_ballot.leader_id);

				}	
			}
			else if(strcmp(data,"PREEMPTED") == 0)
			{
				//received from SCOUT or COMMANDER
				//expects data in the format
				//PREEMPTED:SENDER_ID:BALLOT:

				ballot_str = strtok_r(NULL,DELIMITER,&tok);

				//retrive components of the recv_ballot
				recv_ballot.bnum = atoi(strtok_r(ballot_str,DELIMITER_SEC,&tok1));
				recv_ballot.leader_id = atoi(strtok_r(NULL,DELIMITER_SEC,&tok1));

				if(ballot_compare(recv_ballot,leader_state.ballot) > 0)
				{
					leader_state.lstatus = LEADER_INACTIVE;
					leader_state.ballot.bnum = recv_ballot.bnum + 1;

					//create a new scout thread for the new ballot 					
#if DEBUG==1
						LOG4CXX_TRACE(LeaderLogger,"Leader id: " << my_pid << "creating scout thread " << count_scouts+my_pid*MAX_SCOUTS_PER_LEADER <<" for ballot ("<< leader_state.ballot.bnum << ":" << leader_state.ballot.leader_id << ")\n");
						//printf("Leader id: %d creating scout thread for ballot (%d,%d)!\n",my_pid,leader_state.ballot.bnum,leader_state.ballot.leader_id);
#endif
						//create a new scout thread
						scout_create_args[count_scouts].parent_id = my_pid;
						scout_create_args[count_scouts].my_pid = count_scouts+my_pid*MAX_SCOUTS_PER_LEADER;
						scout_create_args[count_scouts].my_ballot = leader_state.ballot; 
						rc = pthread_create(&scout_thread[count_scouts], NULL, scout, (void *)&scout_create_args[count_scouts]);
						count_scouts++;
				}
			}
			else
			{
				LOG4CXX_TRACE(LeaderLogger,"Leader id: " << my_pid << " undefined msg received"  << data << "\n");
				//printf("undefined msg received at leader id: %d msg:%s\n",my_pid,data);
			}
		}
	}	
return 0;
}
