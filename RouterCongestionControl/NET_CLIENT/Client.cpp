#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <process.h>
#include <time.h>

// ����	----------------------------------------------------
#define SERV_IP			"127.0.0.1"		// IP ����
#define SERV_PORT		7070			// ��Ʈ ����
//---------------------- �輭�� ���� -----------------------
#define BUF_ACK			0				// ���ۿ� ACK�� ���ִ� ��ġ
#define	BUF_TIME		1				// ���ۿ� ���� Ÿ���� ���ִ� ��ġ
#define BUF_MAX_SIZE	2				// ���ۻ����� 100
//-------------------- ��buf ���� ���� ---------------------
#define PACKET_QUEUE	0				// ��Ŷ ť
#define ROUTER_QUEUE	1				// ����� ť
#define	QUEUE_MAX_NUM	2				// ť ����
#define NODE_MAX_NUM	5				// ť�� ũ��
//-------------------- ��ť ���� ���� ----------------------
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
	int Send_Ack;			// ���� ACK

	Queue_pt right_link;	
};
//----------------------------------------------------------



// �Լ� ���� -----------------------------------------------
void PrintError( char * );								// �����޼��� ���
int Router_EnQueue( int );								// �����ť ��ť �Լ� ( int PageNum )
int Router_DeQueue( int );								// �����ť ��ť �Լ� ( int SendAck )
int Packet_EnQueue( Queue_pt );							// ��Ŷť ��ť �Լ� ( queue_pt head )
void Initialize_ALL();									// ���ť �ʱ�ȭ �Լ�
void Initialize_RouterQ();								// �����ť �ʱ�ȭ �Լ�
void PrintIndex();										// �ε��� ��� �Լ�
void PrintQueueInfo();
void PrintMenu();



unsigned WINAPI SendPkt( void* );
unsigned WINAPI RcvPkt( void* );
//----------------------------------------------------------


// �������� ���� -------------------------------------------
SOCKET sHandle;							// ���� ���� �ڵ�
SOCKET cHandle;							// Ŭ���̾�Ʈ ���� �ڵ�

SOCKADDR_IN	sAddr,						// ���ϱ���ü �����ּ�
			cAddr;						// ���ϱ���ü Ŭ���̾�Ʈ

HANDLE	hEvent;

int buf[BUF_MAX_SIZE];					// ������ �ְ�ޱ����� buf�迭
int RCV_ACK;

Queue_Head_pt		queue[ QUEUE_MAX_NUM ];
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


	if( connect( sHandle, (SOCKADDR*)&sAddr, sizeof(sAddr)) == SOCKET_ERROR )
		PrintError("connect() ����");
	printf("[Ŭ���̾�Ʈ]���� �Ϸ�\n");
	Sleep(2 * 1000);
	Initialize_ALL();
	PrintIndex();
	PrintQueueInfo();
	PrintMenu();
	//---------------------------------------------------------------
	HANDLE		SendThread,		// ������ ������
				RcvThread;		// �޴� ������
	//------------------ �� ������ �ڵ� ���� ------------------------
	SendThread=
		(HANDLE)_beginthreadex(NULL, 0, SendPkt, NULL, 0, NULL);
	RcvThread=
		(HANDLE)_beginthreadex(NULL, 0, RcvPkt, NULL, 0, NULL);

	//--------------------- �� ������ ���� --------------------------
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
// ��Ŷ ������ ������
unsigned WINAPI SendPkt(void * arg)	{
	while(true) {
		scanf("%d",&buf[BUF_ACK]);

		send(sHandle, (char*)&buf, sizeof(buf), 0);
		queue[PACKET_QUEUE]->Send_Ack = buf[BUF_ACK];
		if( queue[PACKET_QUEUE]->Base_pageNum > buf[BUF_ACK] )	{
			// �ߺ�ACK���� pageNum�� ������ �����ֱ�����
			queue[PACKET_QUEUE]->Base_pageNum = buf[BUF_ACK];
		}
		for(int i=queue[PACKET_QUEUE]->Base_pageNum; i<=queue[PACKET_QUEUE]->Send_Ack; i++)
			Router_DeQueue( buf[BUF_ACK] );

		Initialize_RouterQ();
		PrintIndex();
		PrintQueueInfo();
		PrintMenu();
		printf(">>>Send ACK : %d \n\n", buf[BUF_ACK]);
		

	}

	return 0;
}

// ��Ŷ �޴� ������
unsigned WINAPI RcvPkt(void * arg)	{
	while(true) {
		recv( sHandle, (char *)&buf, sizeof(buf), 0);
		RCV_ACK = buf[BUF_ACK];
		
		PrintIndex();
		Router_EnQueue( buf[BUF_ACK] );
		PrintQueueInfo();
		PrintMenu();
		printf(">>>Rcv Page : %d\n", RCV_ACK);
		
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
	printf("��                                              Client                      ��\n");
	printf("��                                                                          ��\n");
	printf("��                                                                          ��\n");
	printf("������������������������������������������������������������������������������\n\n");

}


// �ʱ�ȭ �Լ�
void Initialize_ALL()	{
	for(int i=0; i<QUEUE_MAX_NUM; i++)	{
		// ��Ŷť, �����ť �ʱ�ȭ
		queue[i] = (Queue_Head_pt)malloc(sizeof(struct Queue_Head));

		queue[i]->right_link = NULL;
		queue[i]->Base_pageNum = 0;
		queue[i]->Send_Ack = 0;
		queue[i]->cnt_node = 0;
	}

	RCV_ACK = 0;
}
// �����ť �ʱ�ȭ�Լ�
void Initialize_RouterQ()	{
	// �����ť �ʱ�ȭ
	free(queue[ROUTER_QUEUE]);
	queue[ROUTER_QUEUE] = (Queue_Head_pt)malloc(sizeof(struct Queue_Head));

	queue[ROUTER_QUEUE]->right_link = NULL;
	queue[ROUTER_QUEUE]->Base_pageNum = 0;
	queue[ROUTER_QUEUE]->Send_Ack = 0;
	queue[ROUTER_QUEUE]->cnt_node = 0;
}

// ����� ��ť �Լ�
int Router_EnQueue( int pg_num )	{
	Queue_pt new_node = (Queue_pt)malloc(sizeof(struct Queue));	

	if( new_node == NULL )	// �Ҵ������������
		PrintError("�޸� �Ҵ� ����");
	else	{
		// �Ҵ� �޾�����
		if( queue[ROUTER_QUEUE]->cnt_node > NODE_MAX_NUM )	{
			// ����� ť, ���ۿ����÷ο� �������
			printf(">> [���ۿ����÷ο�] ����� ��ť ���� (pagenum: %d)\n", pg_num);
		}
		else	{
			// ����� ť, ���۰� �˳��Ҷ�
			new_node->right_link = new_node->left_link = NULL;
			new_node->pageNum = pg_num;
			
			// �޸�  ����������  �Ҵ� �޾������
			if( queue[ROUTER_QUEUE]->right_link == NULL )	{
				// ù��� �� ���
				queue[ROUTER_QUEUE]->right_link = new_node;
			}
			else	{
				// ť�� ������ �ƴϸ�
				Queue_pt move_temp;				// �ǳ� �̵� ���� move_temp
				move_temp = queue[ROUTER_QUEUE]->right_link;
				
				while( move_temp->right_link != NULL )	{
					// �ǳ� �����̵�
					move_temp = move_temp->right_link;
				}
				move_temp->right_link	= new_node;		// ������ ��� �ڿ� ����� �߰�
				new_node->left_link		= move_temp;	// ������ ������ ��� ����
			}
			
			queue[ROUTER_QUEUE]->cnt_node++;			// �����ť�� ���ī��Ʈ ����
		
		}
	}
	return 0;
}

// ����� ��ť �Լ�
int Router_DeQueue( int Send_Ack )	{
	Queue_pt	dequeue_temp;							// �� �� ��� ������ dequeue_temp
	dequeue_temp = queue[ROUTER_QUEUE]->right_link;

	if(dequeue_temp == NULL)	{
		// ��尡 �ϳ��� ������
		return 0;
	}
	else	{
		// ��尡 �Ѱ��̻��϶�
		if( Send_Ack >= dequeue_temp->pageNum)	{
			// ���� ACK���� �������ѹ��� �۰ų� ���� ��ũ�� �� ��ť
			
			if( dequeue_temp->right_link == NULL )	{
				// �����ť�� ��尡 �Ѱ��϶�			
				queue[ROUTER_QUEUE]->right_link = NULL;
			}
			else	{
				// �����ť�� ��尡 �ΰ� �̻��϶�
				queue[ROUTER_QUEUE]->right_link = dequeue_temp->right_link;
				queue[ROUTER_QUEUE]->right_link->left_link = NULL;
			}

			dequeue_temp->right_link = dequeue_temp->left_link = NULL;	// ��ũ�� NULL�� �ʱ�ȭ
			queue[ROUTER_QUEUE]->cnt_node--;							// ī��Ʈ 1�� ����
			Packet_EnQueue( dequeue_temp );								// ��ť�ȳ�� ��Ŷť�� ��ť
		}
		else	{
			// ���� ACK���� ū ��ũ���� �״�� ����
		}
	}
	
	return 0;
}

// ��Ŷ ��ť �Լ�	
int Packet_EnQueue(Queue_pt head)	{
	if( queue[PACKET_QUEUE]->right_link == NULL )	{
		// ù��� �� ���
		queue[PACKET_QUEUE]->right_link = head;
	}
	else	{
		// ť�� ������ �ƴϸ�
		Queue_pt temp;				// �ǳ� ���� temp
		temp = queue[PACKET_QUEUE]->right_link;
		
		while(true)	{
			if( temp->pageNum == head->pageNum  )	{
				// queue[PACKET_QUEUE] ���� ����� �������ѹ��� ��ť�� �������ѹ��� �ߺ��Ǹ�
				free(head);				// ��ť��Ű���ʰ� free ������
				
				return 0;
			}

			if( temp->right_link == NULL )	{
				// ������ ��� ���� �̵�			
				break;
			}
			else	{
				// �׿��� ���
				temp = temp->right_link;
			}
		}
		temp->right_link	= head;								// ������ ��� �ڿ� ����� �߰�
		head->left_link		= temp;								// ������ ������ ��� ����
	}
	queue[PACKET_QUEUE]->Base_pageNum = (head->pageNum+1);		// ���̽� �������ѹ��� ��Ŷť�� ��ť�� �������ѹ��� �÷��� 1 
	queue[PACKET_QUEUE]->cnt_node++;							// ��Ŷ ť�� ���ī��Ʈ ����

	return 0;
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
	printf("  (�� �����Է� => Send ACK) \n\n");
}