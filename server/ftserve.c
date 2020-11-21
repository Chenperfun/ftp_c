#include "ftserve.h"


int main(int argc, char *argv[])
{	
	int sock_listen, sock_control, port, pid;

	if (argc != 2) {
		printf("usage: ./ftserve port\n");
		exit(0);
	}

	port = atoi(argv[1]);

	/* socket */
	if ((sock_listen = socket_create(port)) < 0 ) {
		perror("Error creating socket");
		exit(1);
	}		
	
	while(1) {	// wait for client request

		// create new socket for control connection
		if ((sock_control = socket_accept(sock_listen))	< 0 )
			break;			
		
		// create child process to do actual file transfer
		if ((pid = fork()) < 0) { 
			perror("Error forking child process");
		} else if (pid == 0) { 
			close(sock_listen);
			ftserve_process(sock_control);		
			close(sock_control);
			exit(0);
		}
		signal(SIGCHLD, SIG_IGN);	
		close(sock_control);
	}

	close(sock_listen);	

	return 0;
}



/**
 * Send file specified in filename over data connection, sending
 * control message over control connection
 * Handles case of null or invalid filename
 */
void ftserve_retr(int sock_control, int sock_data, char* filename)
{	
	FILE* fd = NULL;
	char data[MAXSIZE];
	size_t num_read;							
		
	fd = fopen(filename, "r");
	
	if (!fd) {	
		// send error code (550 Requested action not taken)
		send_response(sock_control, SERVER_REQ_NO_ACK);
		
	} else {	
		// send okay (150 File status okay)
		send_response(sock_control, SERVER_FILE_OK);
	
		do {
			num_read = fread(data, 1, MAXSIZE, fd);

			if (num_read < 0) {
				printf("error in fread()\n");
			}

			// send block
			if (send(sock_data, data, num_read, 0) < 0)
				perror("error sending file\n");

		} while (num_read > 0);													
			
		// send message: 226: closing conn, file transfer successful
		send_response(sock_control, SERVER_CLOSING);

		fclose(fd);
	}
}





/**
 * Send list of files in current directory
 * over data connection
 * Return -1 on error, 0 on success
 */
int ftserve_list(int sock_data, int sock_control)
{
	char data[MAXSIZE];
	size_t num_read;									
	FILE* fd;

	/* list file from line 2 */
	int rs = system("ls -l | tail -n+2 > tmp.txt");
	if ( rs < 0) {
		exit(1);
	}
	
	fd = fopen("tmp.txt", "r");	
	if (!fd) {
		exit(1);
	}

	/* Seek to the beginning of the file */
	fseek(fd, SEEK_SET, 0);

	send_response(sock_control, 1); //starting

	memset(data, 0, MAXSIZE);
	while ((num_read = fread(data, 1, MAXSIZE, fd)) > 0) {
		if (send(sock_data, data, num_read, 0) < 0) {
			perror("err");
		}
		memset(data, 0, MAXSIZE);
	}

	fclose(fd);

	send_response(sock_control, SERVER_CLOSING);	// send 226

	return 0;	
}






/**
 * Open data connection to client 
 * Returns: socket for data connection
 * or -1 on error
 */
int ftserve_start_data_conn(int sock_control)
{
	char buf[1024];	
	int wait, sock_data;

	// Wait for go-ahead on control conn
	if (recv(sock_control, &wait, sizeof wait, 0) < 0 ) {
		perror("Error while waiting");
		return -1;
	}

	// Get client address
	struct sockaddr_in client_addr;
	socklen_t len = sizeof client_addr;
	getpeername(sock_control, (struct sockaddr*)&client_addr, &len);
	inet_ntop(AF_INET, &client_addr.sin_addr, buf, sizeof(buf));

	// Initiate data connection with client
	if ((sock_data = socket_connect(CLIENT_PORT_ID, buf)) < 0)
		return -1;

	return sock_data;		
}





/**
 * Authenticate a user's credentials
 * Return 1 if authenticated,else if 0 - not
 */
int ftserve_check_user(char*user, char*pass)
{
	char username[MAXSIZE];
	char password[MAXSIZE];
	char *pch;
	char buf[MAXSIZE];
	char *line = NULL;
	size_t num_read;									
	size_t len = 0;
	FILE* fd;
	int auth = 0;
	
	fd = fopen(".auth", "r");
	if (fd == NULL) {
		perror("file not found");
		exit(1);
	}	

	/* getline : need user free */
	while ((num_read = getline(&line, &len, fd)) != -1) {
		memset(buf, 0, MAXSIZE);
		strcpy(buf, line);
		
		pch = strtok (buf," ");
		strcpy(username, pch);

		if (pch != NULL) {
			pch = strtok(NULL, " ");
			strcpy(password, pch);
		}

		// remove end of line and whitespace
		trimstr(password, (int)strlen(password));

		if ((strcmp(user,username)==0) && (strcmp(pass,password)==0)) {
			auth = 1;
			break;
		}		
	}
	free(line);	
	fclose(fd);	
	return auth;
}





/** 
 * Log in connected client
 */
int ftserve_login(int sock_control)
{	
	char buf[MAXSIZE];
	char user[MAXSIZE];
	char pass[MAXSIZE];	

	int i = 5;
	int n = 0;

	memset(user, 0, MAXSIZE);
	memset(pass, 0, MAXSIZE);
	memset(buf, 0, MAXSIZE);
	
	// Wait to recieve username by this format: USER username
	if ( (recv_data(sock_control, buf, sizeof(buf)) ) == -1) {
		perror("recv error\n"); 
		exit(1);
	}	

	while (buf[i])
		user[n++] = buf[i++];
	
	// tell client we're ready for password
	send_response(sock_control, SERVER_WAIT_PASS);					
	
	// Wait to recieve password
	memset(buf, 0, MAXSIZE);
	if ( (recv_data(sock_control, buf, sizeof(buf)) ) == -1) {
		perror("recv error\n"); 
		exit(1);
	}
	
	i = 5;
	n = 0;
	while (buf[i] != 0) {
		pass[n++] = buf[i++];
	}
	
	return ftserve_check_user(user, pass);
}





/**
 * Wait for command from client and
 * send response
 * Returns response code
 */
int ftserve_recv_cmd(int sock_control, char*cmd, char*arg)
{	
	int rc = SERVER_CMD_OK;
	char buffer[MAXSIZE];
	
	memset(buffer, 0, MAXSIZE);
	memset(cmd, 0, 5);
	memset(arg, 0, MAXSIZE);
		
	// Wait to recieve command
	if ((recv_data(sock_control, buffer, sizeof(buffer)) ) == -1) {
		perror("recv error\n"); 
		return -1;
	}
	
	strncpy(cmd, buffer, 4);
	char *tmp = buffer + 5;
	strcpy(arg, tmp);
	
	if (strcmp(cmd, "QUIT")==0) {
		rc = SERVER_CLOSED;
	} else if((strcmp(cmd, "USER")==0) || (strcmp(cmd, "PASS")==0) ||
			(strcmp(cmd, "LIST")==0) || (strcmp(cmd, "RETR")==0)|| 
			(strcmp(cmd, "HELP")==0)) {
		rc = SERVER_CMD_OK;
	} else { 
		rc = SERVER_CMD_FAILED;				//invalid command
			}

	send_response(sock_control, rc);	
	return rc;
}

int ftreserve_help(int sock_data, int sock_control)
{
	FILE *fd;
	size_t num_read;
	char data[MAXSIZE];

	fd = fopen(".help","r");
	if(!fd){
		exit(1);
	}

	fseek(fd, SEEK_SET, 0);

	send_response(sock_control, 1); //starting

	memset(data, 0, MAXSIZE);
	while ((num_read = fread(data, 1, MAXSIZE, fd)) > 0)
	{
		if (send(sock_data, data, num_read, 0) < 0) 
			perror("err");
		memset(data, 0, MAXSIZE);
		
	}	
	fclose(fd);
	send_response(sock_control, SERVER_CLOSING);	// send 226

	return 0;	
}

/** 
 * Child process handles connection to client
 */
void ftserve_process(int sock_control)
{
	int sock_data;
	char cmd[5];
	char arg[MAXSIZE];

    /* ready to get user from client */
	send_response(sock_control, SERVER_READY);		

    /* get user and pass from client,and compare with local file ./auth */
	if (ftserve_login(sock_control) == 1) {
		send_response(sock_control, SERVER_GO_ON);	
	} else {
		send_response(sock_control, SERVER_WRONG_PASS);	
		exit(0);
	}	
	
	/* Wait for command */
	while (1) {
		int rc = ftserve_recv_cmd(sock_control, cmd, arg);
		
		/* quit or err */
		if ((rc < 0) || (rc == SERVER_CLOSED)) {
			break;
		}
		/* list or retr */
		if (rc == SERVER_CMD_OK) {
			/* Open data connection with client: return client fd */
			if ((sock_data = ftserve_start_data_conn(sock_control)) < 0) {
				close(sock_control);
				exit(1); 
			}

			/* Execute command */
			if (strcmp(cmd, "LIST")==0) { // Do list
				ftserve_list(sock_data, sock_control);
			} else if (strcmp(cmd, "RETR")==0) { // Do get <filename>
				ftserve_retr(sock_control, sock_data, arg);
			}
			 else if (strcmp(cmd, "HELP")==0) { // Do get <filename>
				ftreserve_help(sock_data, sock_control);
				printf("help.\n");
			}
		
			/* Close client data connection */
			close(sock_data);
		} 
	}
}


