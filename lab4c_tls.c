/*NAME: Cao Xu
EMAIL: cxcharlie@gmail.com
ID: 704551688*/

#ifdef DUMMY
#define MRAA_SUCCESS	0

typedef int mraa_aio_context;
typedef int mraa_result_t;

mraa_aio_context mraa_aio_init(int p){
	(void)p;
	return 1;
}

int mraa_aio_read(mraa_aio_context c)    {
	(void)c;	
	return 650;
}
void mraa_aio_close(mraa_aio_context c)  {
	(void)c;
}

void mraa_deinit(){}
			

#else
#include <mraa.h>
#include <mraa/aio.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define GPIO_TEMPERATURE	0

#define B	4275
#define R0	100000.0

#define EXIT_SUCCESS		0
#define EXIT_FAILURE		1

bool temp_type;//true = F, false = C
int period;
bool logg; //if we have a log or not

bool gen_reports;
mraa_aio_context temps;

float C;
float F;

int lfd;
int sockfd;
char* filename;

int next_time;

//New Paramaters
int id;
char* host_name;
int port_num;
bool hostt;

//SSL Related
SSL* sslClient;
SSL_CTX* newContext;

void exitor()
{
	if (sockfd != -1)
	{
		close(sockfd);
	}
	if (temps != 0)
	{
		mraa_aio_close(temps);
	}
	if (sslClient != NULL)
	{
		SSL_shutdown(sslClient);
		SSL_free(sslClient);
	}
}


float get_temperature(int reading){//provided via Discussion
	float R = 1023.0/((float)reading)-1.0;
	R = R0*R;
	C = 1.0/(log(R/R0)/B + 1/298.15) - 273.15;
	F = (C * 9)/5 + 32;
	if (temp_type)
		return F;
	else
		return C;
}

//
void gettime(bool shutd)
{
	float temperature = get_temperature(mraa_aio_read(temps));

	struct timespec myclock;
	if (clock_gettime(CLOCK_REALTIME, &myclock) != 0)
	{
		fprintf(stderr, "Error occured while getting clocktime: %s", strerror(errno));
		mraa_deinit();
		exit(2);
	}
	struct tm *tm = localtime(&(myclock.tv_sec));
	if(tm == NULL)
	{
		fprintf(stderr, "Error occurred while getting localtime");
		mraa_deinit();
		exit(2);
	}

	next_time = myclock.tv_sec + period;
	bool didsomething = false;
	char output[100];
       	if (gen_reports & !shutd){
		didsomething = true;
		sprintf(output, "%02d:%02d:%02d %.1f\n", tm->tm_hour, tm->tm_min, tm->tm_sec, temperature);
	}
	else if (shutd){
		didsomething = true;
		sprintf(output, "%02d:%02d:%02d SHUTDOWN\n", tm->tm_hour, tm->tm_min, tm->tm_sec);
	}

	int len = strlen(output);

	if (didsomething){
		if(SSL_write(sslClient, output, len) < 0)
		{
			fprintf(stderr, "Error occured while writing: %s", strerror(errno));
			mraa_deinit();
			exit(2);
		}	
	
		if(logg){
		       	if (write(lfd, output, len) < 0){
				fprintf(stderr, "Error occurred while logging %s", strerror(errno));
 				mraa_deinit();
				exit(2);
	       			}
		}
	}	       
	 
}

void shutdown_handler()
{
	gettime(true);
	exit(EXIT_SUCCESS);
}

void cmd_handler(char* cmd){
	if(logg){//TO STDOUT VALID OR NOT
		if(write(lfd, cmd, strlen(cmd)) < 0)
		{
			fprintf(stderr, "Error occurred wihle writing!");
			mraa_deinit();
			exit(2);
		}
		if(write(lfd, "\n", 1) < 0)
		{
			fprintf(stderr, "Error occurred while writing!");
			mraa_deinit();
			exit(2);
		}
	}
	
	if (strncmp(cmd, "SCALE=F", 7) == 0)
	{
	  	temp_type = true;
	}
	else if (strncmp(cmd, "SCALE=C", 7) == 0)
	{
		temp_type = false;
	}
	else if (strncmp(cmd, "PERIOD=", 7) == 0)
	{
		period = atoi(cmd+7);
	}
	else if (strncmp(cmd, "STOP", 4) == 0)
	{
		gen_reports = false;
	}
	else if (strncmp(cmd, "START", 5) == 0)
	{
		gen_reports = true;
	}
	else if (strncmp(cmd, "LOG", 3) == 0)
	{
		//do nuffin
	}
	else if (strncmp(cmd, "OFF", 3) == 0)
	{
		shutdown_handler();
	}

}

//LOG INVALID COMMMANDS!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! 
int main (int argc, char* argv[]) {
	temps = 0; //and temps are initiating
	logg = false;
	sslClient = NULL;
	id = -1;
	hostt = false;
	port_num = -1;
	sockfd = -1;
	period = 1;
	temp_type = true;
	gen_reports = true;

	struct option long_options[] = {
		{"period", required_argument, NULL, 'p'},
		{"scale", required_argument, NULL, 's'},
		{"log", required_argument, NULL, 'l'},
		{"id", required_argument, NULL, 'i'},
		{"host", required_argument, NULL, 'h'},
		{0,0,0,0}
	};
	//check if these are they optional
	int i;
	while ( (i = getopt_long(argc, argv, "", long_options, NULL)) != -1) {
		switch (i) {
			case 'p':
				period = atoi(optarg);
				break;	
			case 's'://chose to let cases were F and C were first char and others were still allowed go through
				if(optarg[0] == 'F')
					temp_type = true;
				else if(optarg[0] == 'C')
					temp_type = false;
				else
				{
					fprintf(stderr, "Error! Invalid parameter for --scale option: %s", optarg);
					exit(EXIT_FAILURE);
				}
				break;
			case 'l':
				logg = true;
				filename = optarg;
				break;
			case 'i':
				if (strlen(optarg) != 9) {
					fprintf(stderr, "Error occurred! Provided ID must be 9 digits long.");
					exit(EXIT_FAILURE);
				}
				else
					id = atoi(optarg);
				break;
			case 'h':
				hostt = true;
				host_name = optarg;
				break;
			default:
				fprintf(stderr, "Error! --period=, --scale=, --id=, --host=, and --log= are only available options");
				exit(EXIT_FAILURE);
				break;
		}
	}
	if (optind >= argc)
	{
		fprintf(stderr, "Error occurred: Must provide port number");
		exit(EXIT_FAILURE);
	}
	else
	{
		port_num = atoi(argv[optind]);
		if (port_num <= 0)
		{
			fprintf(stderr, "Error occurred: Invalid Port Number!");
			exit(EXIT_FAILURE);
		}
	}

	if (!hostt || id == -1 || !logg)
	{
		fprintf(stderr, "Error occurred: --id, --host, and --log are all mandatory options");
		exit(EXIT_FAILURE);
	}

	lfd = creat(filename, 0666);
	if (lfd < 0){
		fprintf(stderr, "Error! Error occurred while running creat()");
		exit(EXIT_FAILURE);
	}	
	//CONNECT
	struct sockaddr_in serv_addr;
	struct hostent* server;

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		fprintf(stderr, "Error occured with socket creation: %s", strerror(errno));
		exit(2);
	}

	server = gethostbyname(host_name);

	memset(&serv_addr, 0, sizeof(struct sockaddr_in));
	serv_addr.sin_family = AF_INET;
	memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
	serv_addr.sin_port = htons(port_num);

	if (connect(sockfd, (struct sockaddr*) & serv_addr, sizeof(serv_addr)) < 0)
	{
		fprintf(stderr, "Error occurred while connecting %s", strerror(errno));
		exit(2);
	}

	//SSL INITIALIZATION FROM SLIDES
	SSL_library_init();
	SSL_load_error_strings();
	OpenSSL_add_all_algorithms();
	newContext = SSL_CTX_new(TLSv1_client_method());
	if (newContext == NULL)
	{
		fprintf(stderr, "Error occurred during SSL initialization"); 
		exit(2);
	}

	sslClient = SSL_new(newContext);
	SSL_set_fd(sslClient, sockfd);
	if (SSL_connect(sslClient) < 0){
		fprintf(stderr, "Error occurred connecting SSL");
		exit(2);
	}

	//SEND ID
	char idd[32];
	sprintf(idd, "ID=%d\n", id);
	if (SSL_write(sslClient, idd, strlen(idd)) < 0)
		{
			fprintf(stderr, "Error occurred while writing to server!");
			exit(2);
		}
	if (logg)
	{
		dprintf(lfd, "ID=%d\n", id);
	}
	
	//Setup Polling for sending commands
	struct pollfd fds[1];
	fds[0].fd = sockfd;//sockfd duped here
	fds[0].events = POLLIN;

	//Setup Temperature_button via MRAA library 
	atexit(exitor);

	
	temps = mraa_aio_init(GPIO_TEMPERATURE);
	if (temps == 0) {
		fprintf(stderr, "Failed to initialize AIO");
		mraa_deinit();
		exit(2);
	}

	char buffer[500]; 
	char extract[501];

	struct timespec myclock;
	
	bool first_time = true;
	while(true)
	{
		if(first_time){
			gettime(false);
			first_time = false;
		}
		
		if (clock_gettime(CLOCK_REALTIME, &myclock) != 0)
			{
				fprintf(stderr, "Error occured while getting clocktime: %s", strerror(errno));
				mraa_deinit();
				exit(2);
			}

		if(!first_time && (myclock.tv_sec >= next_time))
		{	
			gettime(false);
		}

		int poll_res = poll(fds, 1, 1000);
		if(poll_res > 0){
			int bufsize = SSL_read(sslClient, buffer, sizeof(buffer));
			if(bufsize < 0)
			{
				fprintf(stderr, "Error occurred while reading to buffer.");
				mraa_deinit();
				exit(2);
			}
			int j = 0;
			for (int i = 0; i < bufsize; i++)
			{
				if(buffer[i] == '\n')
				{
					extract[j] = '\0';
					cmd_handler(extract);
					j = 0;
				}
				else
				{
					extract[j] = buffer[i];
					j++;
				}	
			}
		}
		else if (poll_res < 0)
		{
			fprintf(stderr, "Erorr occurred while polling: %s", strerror(errno));
			mraa_deinit();
			exit(2);
		}
		//we don't really care about the == 0 case at least for this project	
	
	}
	exit(EXIT_SUCCESS);
}
