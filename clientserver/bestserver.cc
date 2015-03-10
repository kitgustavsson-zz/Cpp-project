
#include "server.h"
#include "connection.h"
#include "connectionclosedexception.h"
#include "memorydatabase.h"
#include "messagehandler.h"
#include "protocol.h"

#include <memory>
#include <iostream>
#include <string>
#include <stdexcept>
#include <cstdlib>
#include <algorithm>

using namespace std;


int main(int argc, char* argv[]){
	

	if(argc != 2) { 
		cerr << "Usage: myserver port-number" << endl;
		exit(1);
	}

	int port = -1;
	try {
		port = stoi(argv[1]);
	} catch (exception& e) {
		cerr << "Wrong port number. " << e.what() << endl;
		exit(1);
	}
	
	Server server(port);
	if (!server.isReady()) {
		cerr << "Server initialization error." << endl;
		exit(1);
	}
	MemoryDatabase database;


	while(true) { 
		auto conn = server.waitForActivity();
		if (conn != nullptr) {
			try {
				MessageHandler msghandler(conn);
				int code = msghandler.recvCode();
				switch(code){
					case Protocol::COM_LIST_NG:
						{
						msghandler.recvCode();
						msghandler.sendCode(Protocol::ANS_LIST_NG);
						vector<Newsgroup> newsgrps = database.getNewsgroups();
						msghandler.sendInt(newsgrps.size());
						int index = 0;

						for_each(newsgrps.begin(), newsgrps.end(), [&msghandler, &index](Newsgroup& grp){
							msghandler.sendIntParameter(index++);
							msghandler.sendStringParameter(grp.title);
						});
							msghandler.sendCode(Protocol::ANS_END);

						break;
						}

					case Protocol::COM_CREATE_NG:
						{
						string grpname = msghandler.recvStringParameter();
						msghandler.recvCode();
						msghandler.sendCode(Protocol::ANS_CREATE_NG);
						if(database.addNewsgroup(grpname)){
							msghandler.sendCode(Protocol::ANS_ACK);
						}else{
							msghandler.sendCode(Protocol::ANS_NAK);
							msghandler.sendCode(Protocol::ERR_NG_ALREADY_EXISTS);
						}
						msghandler.sendCode(Protocol::ANS_END);
						break;
						}
					// 3
					case Protocol::COM_DELETE_NG:
						{
						int grpnr = msghandler.recvIntParameter();
						msghandler.recvCode();
						msghandler.sendCode(Protocol::ANS_DELETE_NG);
						if(database.removeNewsgroup(grpnr)) {
							msghandler.sendCode(Protocol::ANS_ACK);
						} else {
							msghandler.sendCode(Protocol::ANS_NAK);
							msghandler.sendCode(Protocol::ERR_NG_DOES_NOT_EXIST);
						}
						msghandler.sendCode(Protocol::ANS_END);			
						break;
						}					
					// 4
					case Protocol::COM_LIST_ART:
						{
						uint index = msghandler.recvIntParameter();
						msghandler.recvCode();
						msghandler.sendCode(Protocol::ANS_LIST_ART);
						vector<Newsgroup> newsgrps = database.getNewsgroups();
						
						if (index < newsgrps.size()) {
							msghandler.sendCode(Protocol::ANS_ACK);
							
							vector<Article> articles = newsgrps[index].articles;
							msghandler.sendIntParameter(articles.size());

							int count = 0;							
							for_each(articles.begin(), articles.end(), [&count, &msghandler](Article& art){
								msghandler.sendIntParameter(count++);
								msghandler.sendStringParameter(art.title);	
							});
							
						} else {
							msghandler.sendCode(Protocol::ANS_NAK);
							msghandler.sendCode(Protocol::ERR_NG_DOES_NOT_EXIST);
						}						
		
						msghandler.sendCode(Protocol::ANS_END);
						break;		
						}
					// 5
					case Protocol::COM_CREATE_ART:
						{
						uint id = msghandler.recvIntParameter();
						string title = msghandler.recvStringParameter();
						string author = msghandler.recvStringParameter();
						string text = msghandler.recvStringParameter();

						msghandler.recvCode();
						msghandler.sendCode(Protocol::ANS_CREATE_ART);
							
						if (database.addArticle(id,title,author,text)) {
							msghandler.sendCode(Protocol::ANS_ACK);					
						} else {
							msghandler.sendCode(Protocol::ANS_NAK);
							msghandler.sendCode(Protocol::ERR_NG_DOES_NOT_EXIST);
						} 
						msghandler.sendCode(Protocol::ANS_END);
						break;
						}
					// 6
					case Protocol::COM_DELETE_ART:
						{
						uint grpid = msghandler.recvIntParameter();
						uint artid = msghandler.recvIntParameter();

						msghandler.recvCode();
	
						if(database.removeArticle(grpid,artid)) {
							msghandler.sendCode(Protocol::ANS_ACK);
						} else {		
							msghandler.sendCode(Protocol::ANS_NAK);

							if(grpid < database.getNewsgroupCount()) { 
								msghandler.sendCode(Protocol::ERR_NG_DOES_NOT_EXIST);
							} else {
								msghandler.sendCode(Protocol::ERR_ART_DOES_NOT_EXIST);
							}

						}
						
						msghandler.sendCode(Protocol::ANS_END);
						break;
						}
					case Protocol::COM_GET_ART:
						{
						uint grpid = msghandler.recvIntParameter();
						uint artid = msghandler.recvIntParameter();

						msghandler.recvCode();

						if( database.articleExists(grpid, artid)) {
							msghandler.sendCode(Protocol::ANS_ACK);							
							Article art = database.getArticle(grpid,artid);
							msghandler.sendStringParameter(art.title);
							msghandler.sendStringParameter(art.author);
							msghandler.sendStringParameter(art.text);							
						} else {
							msghandler.sendCode(Protocol::ANS_NAK);
							
							if(grpid < database.getNewsgroupCount()) { 
								msghandler.sendCode(Protocol::ERR_NG_DOES_NOT_EXIST);
							} else {
								msghandler.sendCode(Protocol::ERR_ART_DOES_NOT_EXIST);
							}
							
						}
	
					msghandler.sendCode(Protocol::ANS_END);
					break;					
					}
					default:
					
					break;
				}
					


			} catch (ConnectionClosedException&) {
				server.deregisterConnection(conn);
				cout << "Client closed connection" << endl;
			}
			
		} else {
			conn = make_shared<Connection>();
			server.registerConnection(conn);
			cout << "New client connects" << endl;
		}
	}
}