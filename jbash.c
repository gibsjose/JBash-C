//	|jbash.c|: Implements a rudimentary shell in C

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#define LEN 		256		//Arbitrary string length
#define MAX_PARAMS	64		//Maximum number of parameters
//Error codes
#define ERR_EXEC	-100	//Error while executing command
#define ERR_INV		-101	//Invalid/unrecognized command
#define ERR_BASE	-102	//Empty base command
#define ERR_SAVE	-103	//Error while saving command to temp file
#define ERR_GETCMD	-104	//Error while getting command

typedef struct command_t
{
	char base[LEN];
	char full[LEN];
	char params[MAX_PARAMS][LEN];
	unsigned num_params;
	
} command_t;

bool Echo = true;			//Whether or not to echo the user's commands
FILE *History;				//History temp file
struct timeval Begin;		//Beginning time value

static bool check_base(command_t, char *);
int get_command(command_t *);
int str_to_command(char *, command_t *);
int execute_command(command_t);
void print_command(command_t);
int save_command(FILE *, command_t);

//Prints the username followed by a '$' character and a space
static void print_prompt(void)
{
	printf("\n[%s] $ ", getenv("USER"));
}

int main(int argc, char *argv[])
{
	command_t command;		//Command structure
	int err = 0;			//Error code
	
	//Get current time of day
	gettimeofday(&Begin, NULL);
	
	//Clear command structure
	memset(&command, 0, sizeof(command_t));
	
	//Create temp file for history
	History = tmpfile();

	//Loop: Get commands and execute them
	while(1)
	{
		//Print the prompt
		print_prompt();
		
		//Get the command from stdin
		err = get_command(&command);
		
		//Check for errors from getting the command
		if(err == ERR_GETCMD)
		{
			fprintf(stderr, "Error: Could not get command\n");
		}
		
		//Print the command if Echo is true
		print_command(command);
		
		//Save the command to the temp file
		save_command(History, command);
		
		//Execute the command
		err = execute_command(command);
		
		//Check for errors from executing the command
		if(err == ERR_EXEC)
		{
			fprintf(stderr, "Error: Could not execute command\n");
		}
		else if(err == ERR_BASE)
		{
			fprintf(stderr, "Error: Base command empty\n");
		}
		else if(err == ERR_INV)
		{
			fprintf(stderr, "Error: Invalid command\n");
		}
	}
}

//Checks the base value of the command against a given base string
static bool check_base(command_t command, char * base)
{	
	if(!strcmp(command.base, base))
	{
		return true;
	}
	else
	{
		return false;
	}
}

//Use strtok to parse the base command and it's arguments
int get_command(command_t *p_command)
{
	char buf[LEN];
	
	if(fgets(buf, LEN, stdin) == NULL)
	{
		perror("fgets");
		return ERR_GETCMD;
	}
	
	str_to_command(buf, p_command);
	
	return 0;
}

//Use strtok to convert a string into a command
int str_to_command(char *str, command_t *p_command)
{
	char buf[LEN];
	char * base;
	char * param;
	
	base = malloc(LEN);
	param = malloc(LEN);
	
	memset(p_command, 0, sizeof *p_command);
	
	strcpy(p_command->full, str);
	strcpy(buf, str);
	
	int i = 0;
	
	for(base = strtok(buf, " "); i < MAX_PARAMS; i++)
	{
		if((param = strtok(NULL, " ")) == NULL)
		{
			break;
		}
		
		for(int i = 0; i < strlen(param); i++)
		{
			if(param[i] == '\n')
			{
				param[i] = '\0';
			}
		}
		
		strcpy(p_command->params[i], param);
		
		p_command->num_params++;
	}
	
	strcpy(p_command->base, base);
	
	for(int i = 0; i < strlen(p_command->base); i++)
	{
		if(p_command->base[i] == '\n')
		{
			p_command->base[i] = '\0';
		}
	}
	
	//free(base);
	//free(param);
	
	return 0;	
}

int execute_command(command_t command)
{
	char buf[LEN];
	
	if(command.base == NULL)
	{
		return ERR_BASE;
	}
	
	if(check_base(command, "exit"))
	{
		exit(0);
	}
	
	else if(check_base(command, "set"))
	{
		if(!strcmp(command.params[0], "+v"))
		{
			Echo = false;
			return 0;
		}
		
		if(!strcmp(command.params[0], "-v"))
		{
			Echo = true;
			return 0;
		}
		
		return ERR_EXEC;
	}
	
	else if(check_base(command, "size"))
	{
		struct stat st;
		
		printf("\"%s\"\n", command.params[0]);
		
		if(stat(command.params[0], &st))
		{
			perror(command.params[0]);
			
			return errno;
		}
		
		printf("%d bytes\n", (unsigned int)st.st_size);
	}
	
	else if(command.base[0] == '!')
	{
		int n;
		command_t rec_cmd;
		
		rewind(History);
		
		n = atoi(command.base + 1);
		
		for(int i = 0; i < (n - 1); i++)
		{
			fgets(buf, LEN, History);
		}
		
		if(fgets(buf, LEN, History) == NULL)
		{
			fprintf(stderr, "Error: Line %d of history not available\n", n);
		}
		
		puts(buf);
		
		str_to_command(buf, &rec_cmd);
		execute_command(rec_cmd);
		
		fseek(History, 0, SEEK_END);
	}
	
	else if(check_base(command, "history"))
	{
		int i = 1;
		
		rewind(History);
		
		while(fgets(buf, LEN, History) != NULL)
		{
			printf("\t%d\t%s", i++, buf);
		}
	}
	
	else if(check_base(command, "time"))
	{
		struct timeval start;
		struct timeval finish;
		
		memset(buf, 0, LEN);
		
		for(int i = 0; i < command.num_params; i++)
		{
			strcat(buf, command.params[i]);
			strcat(buf, " ");
		}
		
		gettimeofday(&start, NULL);
		
		system(buf);
		
		gettimeofday(&finish, NULL);
		
		printf("\nELAPSED: %dus\n", (int)(finish.tv_usec - start.tv_usec));
	}
	
	else if(check_base(command, "times"))
	{
		struct timeval now;
		
		gettimeofday(&now, NULL);
		
		printf("\nELAPSED: %u.%06us", (unsigned int)(now.tv_sec - Begin.tv_sec), 
			((unsigned int)(now.tv_usec - Begin.tv_usec) % 1000000));
	}
	
	else if(check_base(command, "repeat"))
	{
		memset(buf, 0, LEN);
		
		for(int i = 1; i < command.num_params; i++)
		{
			strcat(buf, command.params[i]);
			strcat(buf, " ");
		}
		
		for(int i = 0; i < atoi(command.params[0]); i++)
		{
			system(buf);
		}
	}
	
	else
	{
		sprintf(buf, "%s", command.full);
		return system(buf);
	}
	
	return 0;
}

void print_command(command_t command)
{
	if(Echo)
	{
		printf("COMMAND: %s", command.full);
	}
}

//Save command to temp file
int save_command(FILE * fd, command_t command)
{
	if(fputs(command.full, fd) == EOF)
	{
		return ERR_SAVE;
	}
	
	return 0;
}
