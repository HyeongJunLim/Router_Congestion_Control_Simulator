#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <process.h>
#include <time.h>

// ����	----------------------------------------------------
#define SERV_IP			"127.0.0.1"		// IP ����
#define SERV_PORT		7070			// ��Ʈ ����
#define TIMER			5				// Ÿ�̸�(��) ���� 
//---------------------- �輭�� ���� -----------------------
#define BUF_ACK			0				// ���ۿ� ACK�� ���ִ� ��ġ
#define	BUF_TIME		1				// ���ۿ� ���� Ÿ���� ���ִ� ��ġ
#define BUF_MAX_SIZE	2				// ���ۻ����� 100
//-------------------- ��buf ���� ���� ---------------------
#define PACKET_QUEUE	0				// ��Ŷ ť
#define ROUTER_QUEUE	1				// ����� ť
#define	QUEUE_MAX_NUM	2				// ť ����
#define NODE_MAX_NUM	10				// ť�� ũ��
//-------------------- ��ť ���� ���� ----------------------
#define DEFAULT_TYPE	'0'				// �⺻
#define PACKET_TYPE		'1'				// ��Ŷ�����
#define ROUTER_TYPE		'2'				// �����
//---------------------- ��Ÿ�� ���� -----------------------
#define	THRESHOLD		10				// �Ӱ���
//---------------------- �谢�� ���� -----------------------


// ����ü ����	--------------------------------------------
typedef struct Queue*			Queue_pt;
typedef struct Queue_Head*		Queue_Head_pt;
//----------------------------------------------------------


// ����ü ����	--------------------------------------------
struct Queue	{
	// ť
	int		pageNum;
	int		start_time;
	Queue_pt right_link, left_link;  	
};
struct Queue_Head	{
	// ��Ŷ ť ���
	int cnt_node;
	int Base_pageNum;		// ���� ù��° ����� pageNum
	int Send_pageNum;		// ���� ������ ����� pageNum
	int start_time;			// ���� �ð�
	int end_time;			// ACK �ð�

	Queue_pt right_link;	
};
//----------------------------------------------------------



// �Լ� ���� -----------------------------------------------
void PrintError( char * );								// �����޼��� ���
int Packet_EnQueue( int );								// ��Ŷť ��ť
int Packet_DeQueue( void );								// ��Ŷť ��ť	-> ����� ��ť����
int Router_EnQueue( Queue_pt );							// ����� ��ť
int Router_DeQueue( void );								// ����� ��ť
int Router_PeekQueue( Queue_pt );						// ����� ��ť
void Initialize_ALL();									// �ʱ�ȭ �Լ�
void PrintIndex();										// �ε��� ����Լ�
void PrintQueueInfo();									// ť���� ����Լ�
void PrintMenu();										// �޴� ��� �Լ�

unsigned WINAPI MakePkt( void* );
unsigned WINAPI SendPkt( void* );
unsigned WINAPI RcvPkt( void* );
//----------------------------------------------------------


// �������� ���� -------------------------------------------
SOCKET sHandle;							// ���� ���� �ڵ�
SOCKET cHandle;							// Ŭ���̾�Ʈ ���� �ڵ�

SOCKADDR_IN	sAddr,						// ���ϱ���ü �����ּ�
			cAddr;						// ���ϱ���ü Ŭ���̾�Ʈ

HANDLE	rqSema;							// �����ť ��������

Queue_Head_pt		queue[ QUEUE_MAX_NUM ];

int buf[BUF_MAX_SIZE];					// Ŭ���̾�Ʈ�� �ְ�ޱ����� buf�迭
int type;								// Ÿ�԰� ����
int Make_Page_Num,						// ������� �������ѹ�
	Ack_Cnt,							// ��ũī��Ʈ
	Window_Size;						// ������ ������
//----------------------------------------------------------


int main(void)
{
	// ���� ���̺귯�� �ʱ�ȭ
	WSADATA	wsa;
	if( WSAStartup( MAKEWORD(2,2), &wsa ) !=0 )
		PrintError("WSAStartup() ����"); 
  
	// ���ϻ��� - TCP
	sHandle = socket( PF_INET, SOCK_STREAM, 0 );
	if( sHandle == INVALID_SOCKET )
		PrintError("TCP ���� ���� ����");
	// ���� �ּ� ����
	memset( &sAddr, 0, sizeof(sAddr) );	

	sAddr.sin_family		=	AF_INET; 
	sAddr.sin_addr.s_addr	=	inet_addr(SERV_IP);
	sAddr.sin_port			=	htons(SERV_PORT);


	if ( bind( sHandle, (SOCKADDR*) &sAddr, sizeof(sAddr) ) == SOCKET_ERROR )
		PrintError("bind() ����");
	if( listen( sHandle, 5 ) == SOCKET_ERROR )
		PrintError("listen() ����");
	

	rqSema	= CreateSemaphore(NULL, 1, 1, NULL);			// �������� ����
	

	Initialize_ALL();
	int cAddrSize;
	while( !(cHandle)  )	{
		cAddrSize = sizeof(cAddr);
		
		cHandle = accept( sHandle, (SOCKADDR*)&cAddr,&cAddrSize );
		
		printf("Connected client IP: %s \n", inet_ntoa(cAddr.sin_addr));
	}
	//---------------------------------------------------------------
	HANDLE		MakeThread, SendThread, RcvThread;
	//------------------ �� ������ �ڵ� ���� ------------------------
	MakeThread=
		(HANDLE)_beginthreadex(NULL, 0, MakePkt, NULL, 0, NULL);
	SendThread=
		(HANDLE)_beginthreadex(NULL, 0, SendPkt, NULL, 0, NULL);
	RcvThread=
		(HANDLE)_beginthreadex(NULL, 0, RcvPkt, NULL, 0, NULL);
	//--------------------- �� ������ ���� --------------------------

	printf("[����]���� �Ϸ�\n");
	Sleep(2 * 1000);
	PrintIndex();
	PrintMenu(); 



	WaitForSingleObject( MakeThread, INFINITE );
	WaitForSingleObject( SendThread, INFINITE );
	WaitForSingleObject( RcvThread, INFINITE );
	


	closesocket( sHandle );

	WSACleanup();


	printf(" >>> ���α׷� ���� ���� �Ϸ�... \n");
	return 0;
}

// �����޼��� ��� �Լ�
void PrintError(char *msg)	{
	printf("> [ERROR] %s \n",msg);
	exit(0);
}

// �ʱ�ȭ �Լ�
void Initialize_ALL()	{
	for(int i=0; i<QUEUE_MAX_NUM; i++)	{
		// ��Ŷť, �����ť �ʱ�ȭ
		queue[i] = (Queue_Head_pt)malloc(sizeof(struct Queue_Head));

		queue[i]->right_link = NULL;
		queue[i]->Base_pageNum = 0;
		queue[i]->Send_pageNum = 0;
		queue[i]->cnt_node = 0;
		queue[i]->start_time = 0;
		queue[i]->end_time = 0;
	}

	Ack_Cnt = 0;
	Window_Size = 1;
}

// ��ť �Լ�
int Packet_EnQueue()	{
	Queue_pt new_node = (Queue_pt)malloc(sizeof(struct Queue));	

	if( new_node == NULL )
		PrintError("�޸� �Ҵ� ����");
	else	{
		new_node->right_link = new_node->left_link = NULL;
		new_node->pageNum = Make_Page_Num;
		
		// �޸�  ����������  �Ҵ� �޾������
		if( queue[PACKET_QUEUE]->right_link == NULL )	{
			// ù��� �� ���
			queue[PACKET_QUEUE]->right_link = new_node;
		}
		else	{
			// ť�� ������ �ƴϸ�
			Queue_pt move_temp;				// �ǳ� �̵� ���� move_temp
			move_temp = queue[PACKET_QUEUE]->right_link;
			
			while( move_temp->right_link != NULL )	{
				// �ǳ� �����̵�
				move_temp = move_temp->right_link;
			}
			move_temp->right_link	= new_node;		// ������ ��� �ڿ� ����� �߰�
			new_node->left_link		= move_temp;	// ������ ������ ��� ����
		}

		Make_Page_Num++;							// ������� �������ѹ� ����
		queue[PACKET_QUEUE]->cnt_node++;			// ��Ŷť�� ���ī��Ʈ ����
	}

	return 0;
}

// ��Ŷ ��ť �Լ�
int Packet_DeQueue()	{
	if( queue[ROUTER_QUEUE]->cnt_node >= NODE_MAX_NUM )	{
		// ����� �ִ� ���� �ʰ������� �����÷ο�
		return 0;
	}
	else	{
		// �ִ���� �ʰ�����������
		Queue_pt	dequeue_temp;			// �� �� ��� ������ dequeue_temp
		dequeue_temp = queue[PACKET_QUEUE]->right_link;

		if( dequeue_temp->right_link == NULL )	{
			// ��尡 �Ѱ����϶�
			queue[PACKET_QUEUE]->right_link = NULL;
		}
		else	{
			// ��尡 �ΰ��̻��϶�
			queue[PACKET_QUEUE]->right_link = dequeue_temp->right_link;
			queue[PACKET_QUEUE]->right_link->left_link = NULL;
		}

		queue[PACKET_QUEUE]->cnt_node--;

		dequeue_temp->right_link = dequeue_temp->left_link = NULL;

		WaitForSingleObject(rqSema, INFINITE);
		Router_EnQueue(dequeue_temp);
		ReleaseSemaphore( rqSema, 1, NULL );	
	}
	
	return 0;
}

// ����� ��ť �Լ�	
int Router_EnQueue(Queue_pt head)	{
	if( queue[ROUTER_QUEUE]->right_link == NULL )	{
		// ù��� �� ���
		queue[ROUTER_QUEUE]->right_link = head;
	}
	else	{
		// ť�� ������ �ƴϸ�
		Queue_pt temp;				// �ǳ� ���� temp
		temp = queue[ROUTER_QUEUE]->right_link;
		
		while( temp->right_link != NULL )	{
			// �ǳ� �����̵�
			temp = temp->right_link;
		}
		temp->right_link	= head;		// ������ ��� �ڿ� ����� �߰�
		head->left_link		= temp;		// ������ ������ ��� ����
	}

	queue[ROUTER_QUEUE]->cnt_node++;												// ����� ť�� ���ī��Ʈ ����

	return 0;
}


// ����� ��ũ ť �Լ�
int Router_PeekQueue(Queue_pt tmp)	{
	if( tmp == NULL )	// ������ 1���� ������
		return 0;
	else	{
		// ������ 1�� �̻��϶�
		buf[BUF_ACK] = tmp->pageNum;
		queue[ROUTER_QUEUE]->Send_pageNum = tmp->pageNum;			// ������ ��� �������ѹ� ���
	
		tmp->start_time = (int)time(NULL);							// send �Ǿ������� �ð��� ���
		queue[ROUTER_QUEUE]->start_time = tmp->start_time;			// ���� �ð� �Է�
		queue[ROUTER_QUEUE]->end_time	= tmp->start_time;			// ����ȭ
	}
	
	return 0;
}
 
// ACK üũ �Լ�
int Ack_Check( int ack )	{
	if( ack >= queue[ROUTER_QUEUE]->Base_pageNum && ack <= queue[ROUTER_QUEUE]->Send_pageNum  )	{
		// ���� ack�� ���� �������ѹ��� ù��°�� ������ ���̿� ������
		if( queue[ROUTER_QUEUE]->end_time - queue[ROUTER_QUEUE]->start_time > TIMER )	{
			// ack�� �޾������� �ð��� ���� ���� �ð����� Ŭ��, �� Ÿ�Ӿƿ��϶�
			printf(">>> [Ÿ�Ӿƿ�]������ �ѹ� : %d \n", ack);

			Window_Size = 1;	// Ÿ�Ӿƿ��� �ɰ��ѹ����̹Ƿ� ����������� 1�� ����
		}
		else	{
			// �������϶�, �� Ÿ�Ӿƿ�X
			for(int i=queue[ROUTER_QUEUE]->Base_pageNum; i<=ack;i++)
				Router_DeQueue();
			WaitForSingleObject(rqSema, INFINITE);
			queue[ROUTER_QUEUE]->Base_pageNum = (ack+1);
			ReleaseSemaphore( rqSema, 1, NULL );
		

			if( Window_Size < THRESHOLD )	{
				// ������ ����� �Ӱ�ġ���� ������
				Window_Size *= 2;	// ������ ������ ��������
			}
			else	{
				// ������ ����� �Ӱ�ġ���� ũ��
				Window_Size++;		// ������ ������ ��������
			}

			PrintIndex();
			PrintQueueInfo();
			PrintMenu(); 
			printf("[ RCV_INFO ] ���������������������������������������������������� \n");
			printf(">>> Rcv pkt : %d \n", ack);
			printf(">>> [��ť�Ϸ�]����������� ����(���������������:%d)\n", Window_Size);
			
		}
	}
	else if( ack < queue[ROUTER_QUEUE]->Base_pageNum )	{
		// ���� ��������ȣ���� ���� ACK ������, ��, �̹� �Ѿ ������������ ACK�� �޾�����
		Ack_Cnt++;		// ��ũ ī��Ʈ ����
	}
	else	{
		// ���� ������� ack�� ������
		printf(">>> [��ȿ�������� ACK] ACK num : %d \n", ack);
	}
	

	if( Ack_Cnt >= 3 )	{
		// �ߺ�ACK�� 3���̻� �߻�������
		if(Window_Size == 1)	{
			// ������ ������ 1�ϰ��
			Window_Size = 1;	// �״�� 1�� ����. 2�γ����� 0�Ǳ⶧��
		}
		else	{
			// ������ ������ 2�̻��� ���
			Window_Size /= 2;	// ������ ������ ��������
		}
		printf(">>> [�ߺ�ACK] pagenum : %d \n", ack);
		Ack_Cnt = 0;			// �ٽ� 0���� �ʱ�ȭ
	}

	return 0;
}

// ����� ��ť �Լ�
int Router_DeQueue()	{
	// �Ǿ� ��� ��ť
	Queue_pt delete_temp;							// �Ǿ� ��� ������ delete_temp
	delete_temp = queue[ROUTER_QUEUE]->right_link;
	
	if(delete_temp == NULL)	{
		// �����ť�� ��尡 �ϳ���������
		return 0;
	}
	else if( delete_temp->right_link == NULL )	{
		// �����ť�� ��尡 �ϳ�������
		queue[ROUTER_QUEUE]->right_link = NULL;
	}
	else	{
		// �����ť�� ��尡 �ΰ� �̻�������
		queue[ROUTER_QUEUE]->right_link = delete_temp->right_link;
		queue[ROUTER_QUEUE]->right_link->left_link = NULL;
	}


	queue[ROUTER_QUEUE]->cnt_node--;

	free(delete_temp);
	
	return 0;
}

// ��Ŷ ����� ������
unsigned WINAPI MakePkt(void * arg)	{
	while(true)	{
		scanf("%c", &type);
		fflush(stdin);

		if( type == PACKET_TYPE )	{
			Packet_EnQueue();	// ��Ŷť�� ��ť
			Packet_DeQueue();	// ��Ŷť�� ��ť ��, �����ť�� ��ť
		
			PrintIndex();
			PrintQueueInfo();
			PrintMenu(); 
		}
	}
	return 0;
}

// ��Ŷ ������ ������
unsigned WINAPI SendPkt(void * arg)	{
	Queue_pt tmp = (Queue_pt)malloc(sizeof(struct Queue));	

	
	while(true) {
	
		while( type == ROUTER_TYPE )	{
			WaitForSingleObject(rqSema, INFINITE);
			tmp = queue[ROUTER_QUEUE]->right_link;
		
			PrintIndex();
			PrintQueueInfo();
			PrintMenu(); 
			printf("[ SEND_INFO ] �������������������������������������������������� \n");
			printf(">>> ���� ������ ������ : %d \n", Window_Size);
			
			for(int i=0; (tmp!=NULL)&&i<Window_Size; i++)	{
				Router_PeekQueue( tmp );
				
				printf(">>> Send pkt : %d \n", tmp->pageNum);
				send( cHandle, (char *)buf, sizeof(buf), 0 );
				tmp = tmp->right_link;
			}
			ReleaseSemaphore(rqSema, 1, NULL);
				
			type = DEFAULT_TYPE;
		}
	}

	return 0;
}

// ��Ŷ �޴� ������
unsigned WINAPI RcvPkt(void * arg)	{
	int strlen;
	
	while(true)	{
		strlen = recv( cHandle, (char *)&buf, sizeof(buf), 0);
		if(strlen == -1)
			return -1;

		buf[BUF_TIME] = (int)time(NULL);
		queue[ROUTER_QUEUE]->end_time = buf[BUF_TIME];

	
		PrintIndex();
		PrintQueueInfo();
		PrintMenu(); 
		printf("[ RCV_INFO ] ���������������������������������������������������� \n");
		printf(">>> Rcv pkt : %d \n", buf[BUF_ACK])	;	
		Ack_Check( buf[BUF_ACK] );
	}

	return 0;
}


// �ε��� ��� �Լ�
void PrintIndex()	{
	system("cls");
	printf("������������������������������������������������������������������������������\n");
	printf("��                                                                          ��\n");
	printf("��                                                                          ��\n");
	printf("��                Congestion Control                                        ��\n");
	printf("��                                              Server                      ��\n");
	printf("��                                                                          ��\n");
	printf("��                                                                          ��\n");
	printf("������������������������������������������������������������������������������\n\n");

}
// ť���� ����Լ�
void PrintQueueInfo()	{
	Queue_pt packet_temp = queue[PACKET_QUEUE]->right_link;
	Queue_pt router_temp = queue[ROUTER_QUEUE]->right_link;

	printf("[ PACKET_QUEUE INFO ] ���������������������������������������������������������� \n        ");
	if(queue[PACKET_QUEUE]->cnt_node == 0)	{
		// ��尡 0���̸�
		printf("[ N U L L ] \n");
	}
	else if(queue[PACKET_QUEUE]->cnt_node < 8)	{
		//��Ŷť ������ 8 �� �̸��̸�
		while( packet_temp != NULL )	{
			printf("%d -> ", packet_temp->pageNum);
			packet_temp = packet_temp->right_link;
		}
		printf("[ E N D ] \n");

	}
	else	{
		//��Ŷť ������ 8 �� �̻��̸�
		printf("%d -> ", packet_temp->pageNum);
		while( packet_temp->right_link != NULL)	{
			// ������ ��� ���� �̵�
			packet_temp = packet_temp->right_link;
		}
		printf("     ................     -> %d -> ", packet_temp->pageNum);
		printf("[ E N D ] \n");

	}
	
	printf("\n");

	printf("[ ROUTER_QUEUE INFO ] ���������������������������������������������������������� \n        ");
	if(queue[ROUTER_QUEUE]->cnt_node == 0)	{
		// ��尡 0���̸�
		printf("[ N U L L ] \n");
	}
	else if(queue[ROUTER_QUEUE]->cnt_node < 8)	{
		// �����ť ������ 8 �� �̸��̸�
		while( router_temp != NULL )	{
			printf("%d -> ",  router_temp->pageNum);
			router_temp = router_temp->right_link;
		}
		printf("[ E N D ] \n");
	}
	else	{
		// �����ť ������ 8 �� �̻��̸�
		printf("%d -> ",  router_temp->pageNum);
		while( router_temp->right_link != NULL)	{
			// ������ ��� ���� �̵�
			router_temp = router_temp->right_link;
		}
		printf("     ................     -> %d -> ", router_temp->pageNum);
		printf("[ E N D ] \n");
	}
}

// �޴� ��� �Լ�
void PrintMenu()	{
	printf("\n");
	printf("[ M E N U ] �������������������������������������������������������������������� \n");
	printf("  1. ��Ŷ ����� & �������ť \n");
	printf("  2. ��Ŷ ���� \n\n");
}