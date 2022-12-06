#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <strings.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

#define MAX_INPUT_SIZE 1024
#define MAX_TOKEN_SIZE 64
#define MAX_NUM_TOKENS 64
#define MAX_BACKGROUND_PROCESS 64

char **tokenize(char *);
void add(pid_t);
int delete (pid_t);
void cd(char *[], int);
void execBack(char *[]);
void execFore(char *[]);
void kill_processes();
void sigint_handler(int);
void sigchld_handler(int);
void free_memory(char **);

struct HashNode
{
	pid_t pid;
	struct HashNode *next;
};

typedef struct HashNode HashNode;
HashNode *hashTable[MAX_BACKGROUND_PROCESS];
pid_t foregroundPID = 0;

int main(int argc, char *argv[])
{
	signal(SIGCHLD, sigchld_handler);
	signal(SIGINT, sigint_handler);
	char line[MAX_INPUT_SIZE], path[MAX_TOKEN_SIZE], **tokens;

	while (1)
	{
		// Taking input
		bzero(line, sizeof(line));
		printf("%s:%s$ ", getlogin(), getcwd(path, sizeof(path)));
		scanf("%[^\n]", line);
		getchar();
		line[strlen(line)] = '\n'; // terminate with new line
		tokens = tokenize(line);
		int tokenCount = atoi(tokens[0]);

		if (tokenCount == 0)
			continue;

		// Execution
		if (strcmp(tokens[1], "exit") == 0)
		{
			kill_processes();
			while (1)
			{
				pid_t reaped_child = waitpid(-1, NULL, 0);
				if (reaped_child <= 0)
					break;
				printf("Shell: Background process finished\n");
			}
			free_memory(tokens); // Freeing the allocated memory before exiting
			exit(EXIT_SUCCESS);
		}
		else if (strcmp(tokens[1], "cd") == 0)
		{
			cd(tokens + 1, tokenCount);
		}
		else if (strcmp(tokens[tokenCount], "&") == 0)
		{
			tokens[tokenCount] = NULL;
			execBack(tokens + 1); // Execute in background
		}
		else
		{
			execFore(tokens + 1); // Execute in Foreground
		}

		free_memory(tokens); // Freeing the allocated memory
	}
	return 0;
}

void execFore(char *tokens[])
{
	foregroundPID = fork();

	if (foregroundPID == 0)
	{
		execvp(tokens[0], tokens);
		perror("Exec Error: ");
		exit(EXIT_FAILURE);
	}

	waitpid(foregroundPID, NULL, 0);
	foregroundPID = 0;
}

void execBack(char *tokens[])
{
	int backgroundPID = fork();
	if (backgroundPID == 0)
	{
		setpgrp();
		execvp(tokens[0], tokens);
		perror("Exec Error: ");
		exit(EXIT_FAILURE);
	}

	add(backgroundPID);
}

void cd(char *tokens[], int tokenCount)
{
	if (tokenCount > 2)
	{
		printf("Error: Too many arguments\n");
		return;
	}

	if (strcmp(tokens[1], "~") == 0)
		strcpy(strcpy(tokens[1], "/home/") + 6, getlogin());

	int status = chdir(tokens[1]);
	if (status)
	{
		perror("Error");
	}
}

void kill_processes()
{
	for (int i = 0; i < 64; i++)
	{
		HashNode *curr = hashTable[i];
		while (curr != NULL)
		{
			kill(curr->pid, SIGTERM);
			curr = curr->next;
		}
	}
}

void sigchld_handler(int sigchld)
{
	pid_t reaped_child;
	while ((reaped_child = waitpid(-1, NULL, WNOHANG)) > 0)
	{
		if (delete (reaped_child))
			printf("Shell: Background process finished\n");
	}
}

void sigint_handler(int sigint)
{
	if (foregroundPID != 0)
		kill(foregroundPID, SIGTERM);
}

// tokens[0] = number of tokens, actual tokens starts from token[1]
char **tokenize(char *line)
{
	char **tokens = (char **)malloc(MAX_NUM_TOKENS * sizeof(char *));
	char *token = (char *)malloc(MAX_TOKEN_SIZE * sizeof(char));
	int i, tokenIndex = 0, tokenNo = 1;

	for (i = 0; i < strlen(line); i++)
	{
		char readChar = line[i];
		if (readChar == ' ' || readChar == '\n' || readChar == '\t')
		{
			token[tokenIndex] = '\0';
			if (tokenIndex != 0)
			{
				tokens[tokenNo] = (char *)malloc(MAX_TOKEN_SIZE * sizeof(char));
				strcpy(tokens[tokenNo++], token);
				tokenIndex = 0;
			}
		}
		else
		{
			token[tokenIndex++] = readChar;
		}
	}

	free(token);
	tokens[0] = (char *)malloc(MAX_TOKEN_SIZE * sizeof(char));
	sprintf(tokens[0], "%d", tokenNo - 1);
	tokens[tokenNo] = NULL;
	return tokens;
}

void add(pid_t pid)
{
	int index = pid % MAX_BACKGROUND_PROCESS;
	HashNode *newNode = (HashNode *)malloc(sizeof(HashNode));
	newNode->pid = pid;
	newNode->next = hashTable[index];
	hashTable[index] = newNode;
}

int delete (pid_t pid)
{
	int index = pid % MAX_BACKGROUND_PROCESS;
	HashNode *prev = NULL, *curr = hashTable[index];

	while (curr != NULL)
	{
		if (curr->pid == pid)
		{
			if (prev == NULL)
				hashTable[index] = curr->next;
			else
				prev->next = curr->next;

			free(curr);
			return 1;
		}

		prev = curr;
		curr = curr->next;
	}

	return 0;
}

void free_memory(char **tokens)
{
	for (int i = 0; tokens[i] != NULL; i++)
		free(tokens[i]);
	free(tokens);
}