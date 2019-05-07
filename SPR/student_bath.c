#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <mqueue.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#define QUEUE_NAME "/m_queue"
#define MAX_SIZE 20
#define BATH_CAPACITY 2


typedef enum {
    MALE = 0,
    FEMALE = 7
} Gender;

typedef struct Student {
    char name[120];
    bool isWashed;
    Gender type;
    struct Student * next;
} node;

struct BathRoom {
	int capacity;
	bool isFree; //unused
};

const struct BathRoom bathroom = {BATH_CAPACITY, false};
node * head = NULL;
char * buffer;
char * washedStudents;
mqd_t  mq = {0};
int results;
char (* students)[148];
pthread_t tid[2];

void addNode(node * head, char * name, Gender gender);
void readData(char * buffer, int fd); 
int parseInputData();
void loadListData();
Gender validateStringToEnum(char * input);
void* washMales(void *arg);
void* washFemales(void *arg);
bool canStartWashing(node * head, Gender gender);
void startWashing(node * head, Gender gender);
void cleanString(char * str, char remove); 
bool isStudentWashed(char * name);


int main() {
    head = (struct Student * ) malloc(sizeof(struct Student));
    struct mq_attr attr = {0};
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = 20;
    attr.mq_curmsgs = 0;
	results = open("washed_students.txt", O_CREAT | O_RDWR);	
	loadListData(head);
    mq = mq_open(QUEUE_NAME, O_CREAT | O_RDWR | O_NONBLOCK, 0700, &attr);
    pthread_create(&tid[0], NULL, washMales, NULL);
    pthread_create(&tid[1], NULL, washFemales, NULL);
    pthread_join(tid[1], NULL);
    pthread_join(tid[0], NULL);
    return 0;
}

void* washMales(void *arg) {
	char buffer[MAX_SIZE] = {0};
	ssize_t bytes_read = 0;
	char data[MAX_SIZE] = "SEND!";
    if (mq > -1) {
        if(canStartWashing(head, MALE)) {
            startWashing(head, MALE);
            mq_send(mq, data, MAX_SIZE, 0);
        } else {
            while(1) {
                bytes_read = mq_receive(mq, buffer, MAX_SIZE, NULL);
                if (bytes_read > 0 && strcmp(data, buffer) == 0)
                {
				    startWashing(head, MALE);
					(void) mq_close(mq);  
					(void) close(results);
					free(students);
					break;
                }
            }
		}
    }
    else {
        (void) printf("Receiver: Failed to load queue: %s\n", strerror(errno));
    }
    return NULL;
}

void* washFemales(void *arg) {
	char buffer[MAX_SIZE] = {0};
	ssize_t bytes_read = 0;
	char data[20] = "SEND!";
    if (mq > -1) {
        if(canStartWashing(head, FEMALE)) {
            startWashing(head, FEMALE);
            mq_send(mq, data, MAX_SIZE, 0);
        } else {
            while(1) {
                bytes_read = mq_receive(mq, buffer, MAX_SIZE, NULL);
                if (bytes_read > 0 && strcmp(data, buffer) == 0)
                {
				    startWashing(head, FEMALE);
					(void) mq_close(mq);
					(void) close(results);
					free(students);
                    break;
                }
              }
	       }
        }
        else {
            (void) printf("Receiver: Failed to load queue: %s\n", strerror(errno));
        }
        return NULL;
    }

    void startWashing(node * head, Gender gender) {
        node * temp = head;
        char* genderType;
		int subGroup = 0;
        if(gender == MALE) {
            genderType = "Males";
        } else {
            genderType = "Females";
        }
		int pendingStudents = 0;
		while (temp != NULL) {
            if(temp -> type == gender && strlen(temp -> name) > 1) {
				pendingStudents++;	
            }
            temp = temp -> next;
		}
		temp = head;
		if( pendingStudents == 0) {
			  printf("\n*** No %s to wash! ***\n", genderType);
			  return;	
		}
        printf("\n***%s entered the bathroom(BATH LOCKED!) ***:\n\n", genderType);
        while (temp != NULL) {
            if(temp -> type == gender) {
				if(subGroup == 0) {
					printf("\n*Subgroup entered:\n");
				}
				printf("\t%s\n", temp -> name);
				write(results, "\n", 1);	
				write(results, temp -> name, strlen(temp -> name));
				if(subGroup == bathroom.capacity - 1 || pendingStudents < bathroom.capacity) {
					subGroup = 0;
					printf("\n Washing started...\n");
					(void)sleep(1);
					printf("\n*Subgroup leaved!\n");
					pendingStudents -= bathroom.capacity;
				} else {
					subGroup++;		
				}
            }
            temp = temp -> next;
        }
        printf("\n***%s leaved the bathroom...(BATH UNLOCKED!) ***\n", genderType);
    }
	
    bool canStartWashing(node * head, Gender gender) {
		int inputGender = 0;
		int otherGender = 0;
        node * temp = head;
        while (temp != NULL) {
            if(temp -> type == gender) {
                inputGender++;
            } else {
                otherGender++;
            }
            temp = temp -> next;
        }
        if(inputGender > otherGender) {
            return true;
        } else if(inputGender < otherGender) {
            return false;
        } else if(inputGender == otherGender) {
            if(gender == FEMALE) {
                return true;
            } else {
                return false;
            }
        }
    }

    void addNode(node * head, char * name, Gender gender) {
        node * p = head;
        node * temp = (struct Student *) malloc(sizeof(struct Student));
        strcpy(temp -> name, name);
        temp -> type = gender;
        if (head == NULL) {
            head = temp;
            return;
        }
        while (p -> next != NULL) {
            p = p -> next;
        }
        p -> next = temp;
        temp -> next = NULL;
    }

    void readData(char * buffer, int fd) {
		int buffSize = 0;
        char ch;
        int i = 0;
        while (read(fd, & ch, 1) == 1) {
            buffSize++;
            buffer = (char *) realloc(buffer, buffSize * sizeof(char));
            buffer[i] = ch;
            i++;
        }
    }

    int parseInputData() {
        char delim[] = ";";
	    int fd = open("students.txt" , O_RDONLY);
		if (fd < 0) {
            perror("Error opening file...");
            exit(0);
        }
		buffer = (char *) malloc(10 * sizeof(char));
		readData(buffer, fd);
		close(fd);
		int studentSize = 0;
		for(int m = 0 ; m < strlen(buffer) ; m++){
			if(buffer[m] == ';'){
				studentSize++;
			}		
		}
		students = malloc(studentSize * sizeof(char*));
        char * ptr = strtok(buffer, delim);
		int tempSize = 0;
        while (ptr != NULL) {
            bool containsChar = false;
            for(int i = 0; i < strlen(ptr); i++) {
                if(ptr[i] == 'M') {
                    containsChar = true;
                }
            }
            if (!containsChar) {
                break;
            }
			strcpy(students[tempSize], ptr);
            tempSize++;
            ptr = strtok(NULL, delim);
        }
		return studentSize;
    }

    void loadListData(node * head) {
		washedStudents = (char*) malloc(10);
		readData(washedStudents, results);
        int studentsLen = parseInputData();
        char delim[] = ",";
        char nameGender[2][120];
		bool heapInit = false;
        for (int j = 0; j < studentsLen; j++) {
            char * item = strtok(students[j], delim);
            int i = 0;
            while (item != NULL) { // load name and gender for a student...
                strcpy(nameGender[i], item);
                item = strtok(NULL, delim);
                i++;
            }
			cleanString(nameGender[0], '\n');
			if(!isStudentWashed(nameGender[0])){
		        if (!heapInit) {
		            strcpy(head -> name, nameGender[0]);
		            head -> type = validateStringToEnum(nameGender[1]);
					heapInit = true;	
		        } else {
		            addNode(head, nameGender[0], validateStringToEnum(nameGender[1]));
		        }
			}
        }
		free(buffer);
		free(washedStudents);
    }

    void cleanString(char * str, char remove) {
        int count = 0;
        for (int i = 0; str[i]; i++) {
            if (str[i] != remove) {
                str[count++] = str[i];
            }
        }
        str[count] = '\0';
    }

    Gender validateStringToEnum(char * input) {
        cleanString(input, ' ');
        if (strcmp(input, "MALE") == 0) {
            return MALE;
        } else {
            return FEMALE;
        }
    }
	
	bool isStudentWashed(char * name) {
		if(strstr(washedStudents, name) != NULL) {
    		return true;
		}
		return false;
	}

