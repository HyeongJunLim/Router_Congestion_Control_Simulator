#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <process.h>
#include <time.h>

// 정의	----------------------------------------------------
#define SERV_IP			"127.0.0.1"		// IP 정의
#define SERV_PORT		7070			// 포트 정의
//---------------------- ↑서버 설정 -----------------------
#define BUF_ACK			0				// 버퍼에 ACK가 들어가있는 위치
#define	BUF_TIME		1				// 버퍼에 수신 타임이 들어가있는 위치
#define BUF_MAX_SIZE	2				// 버퍼사이즈 100
//-------------------- ↑buf 관련 정의 ---------------------
#define PACKET_QUEUE	0				// 패킷 큐
#define ROUTER_QUEUE	1				// 라우터 큐
#define	QUEUE_MAX_NUM	2				// 큐 갯수
#define NODE_MAX_NUM	5				// 큐의 크기
//-------------------- ↑큐 관련 정의 ----------------------
//---------------------- ↑각종 값들 -----------------------


// 구조체 정의	--------------------------------------------
typedef struct Queue*			Queue_pt;
typedef struct Queue_Head*		Queue_Head_pt;
//----------------------------------------------------------


// 구조체 선언	--------------------------------------------
struct Queue	{
	// 큐
	int		pageNum;
	int		start_time;
	Queue_pt right_link, left_link;  	
};
struct Queue_Head	{
	// 패킷 큐 헤드
	int cnt_node;
	int Base_pageNum;		// 보낸 첫번째 노드의 pageNum
	int Send_Ack;			// 보낼 ACK

	Queue_pt right_link;	
};
//----------------------------------------------------------



// 함수 정의 -----------------------------------------------
void PrintError( char * );								// 에러메세지 출력
int Router_EnQueue( int );								// 라우터큐 인큐 함수 ( int PageNum )
int Router_DeQueue( int );								// 라우터큐 디큐 함수 ( int SendAck )
int Packet_EnQueue( Queue_pt );							// 패킷큐 인큐 함수 ( queue_pt head )
void Initialize_ALL();									// 모든큐 초기화 함수
void Initialize_RouterQ();								// 라우터큐 초기화 함수
void PrintIndex();										// 인덱스 출력 함수
void PrintQueueInfo();
void PrintMenu();



unsigned WINAPI SendPkt( void* );
unsigned WINAPI RcvPkt( void* );
//----------------------------------------------------------


// 전역변수 정의 -------------------------------------------
SOCKET sHandle;							// 서버 소켓 핸들
SOCKET cHandle;							// 클라이언트 소켓 핸들

SOCKADDR_IN	sAddr,						// 소켓구조체 서버주소
			cAddr;						// 소켓구조체 클라이언트

HANDLE	hEvent;

int buf[BUF_MAX_SIZE];					// 서버와 주고받기위한 buf배열
int RCV_ACK;

Queue_Head_pt		queue[ QUEUE_MAX_NUM ];
//----------------------------------------------------------


int main(void)
{
	// 소켓 라이브러리 초기화
	WSADATA	wsa;
	if( WSAStartup( MAKEWORD(2,2), &wsa ) !=0 )
		PrintError("WSAStartup() 실패"); 
  
	// 소켓생성 - TCP
	sHandle = socket( PF_INET, SOCK_STREAM, 0 );
	if( sHandle == INVALID_SOCKET )
		PrintError("TCP 소켓 생성 실패");
	// 서버 주소 설정
	memset( &sAddr, 0, sizeof(sAddr) );	

	sAddr.sin_family		=	AF_INET; 
	sAddr.sin_addr.s_addr	=	inet_addr(SERV_IP);
	sAddr.sin_port			=	htons(SERV_PORT);


	if( connect( sHandle, (SOCKADDR*)&sAddr, sizeof(sAddr)) == SOCKET_ERROR )
		PrintError("connect() 에러");
	printf("[클라이언트]연결 완료\n");
	Sleep(2 * 1000);
	Initialize_ALL();
	PrintIndex();
	PrintQueueInfo();
	PrintMenu();
	//---------------------------------------------------------------
	HANDLE		SendThread,		// 보내는 쓰레드
				RcvThread;		// 받는 쓰레드
	//------------------ ↑ 쓰레드 핸들 선언 ------------------------
	SendThread=
		(HANDLE)_beginthreadex(NULL, 0, SendPkt, NULL, 0, NULL);
	RcvThread=
		(HANDLE)_beginthreadex(NULL, 0, RcvPkt, NULL, 0, NULL);

	//--------------------- ↑ 쓰레드 생성 --------------------------
	WaitForSingleObject( SendThread, INFINITE );
	WaitForSingleObject( RcvThread, INFINITE );
	


	closesocket( sHandle );

	WSACleanup();


	printf(" >>> 프로그램 정상 종료 완료... \n");
	return 0;
}

// 에러메세지 출력 함수
void PrintError(char *msg)	{
	printf("> [ERROR] %s \n",msg);
	exit(0);
}
// 패킷 보내는 쓰레드
unsigned WINAPI SendPkt(void * arg)	{
	while(true) {
		scanf("%d",&buf[BUF_ACK]);

		send(sHandle, (char*)&buf, sizeof(buf), 0);
		queue[PACKET_QUEUE]->Send_Ack = buf[BUF_ACK];
		if( queue[PACKET_QUEUE]->Base_pageNum > buf[BUF_ACK] )	{
			// 중복ACK후의 pageNum의 오류를 고쳐주기위함
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

// 패킷 받는 쓰레드
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

// 인덱스 출력 함수
void PrintIndex()	{
	system("cls");
	printf("┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓\n");
	printf("┃                                                                          ┃\n");
	printf("┃                                                                          ┃\n");
	printf("┃                Congestion Control                                        ┃\n");
	printf("┃                                              Client                      ┃\n");
	printf("┃                                                                          ┃\n");
	printf("┃                                                                          ┃\n");
	printf("┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛\n\n");

}


// 초기화 함수
void Initialize_ALL()	{
	for(int i=0; i<QUEUE_MAX_NUM; i++)	{
		// 패킷큐, 라우터큐 초기화
		queue[i] = (Queue_Head_pt)malloc(sizeof(struct Queue_Head));

		queue[i]->right_link = NULL;
		queue[i]->Base_pageNum = 0;
		queue[i]->Send_Ack = 0;
		queue[i]->cnt_node = 0;
	}

	RCV_ACK = 0;
}
// 라우터큐 초기화함수
void Initialize_RouterQ()	{
	// 라우터큐 초기화
	free(queue[ROUTER_QUEUE]);
	queue[ROUTER_QUEUE] = (Queue_Head_pt)malloc(sizeof(struct Queue_Head));

	queue[ROUTER_QUEUE]->right_link = NULL;
	queue[ROUTER_QUEUE]->Base_pageNum = 0;
	queue[ROUTER_QUEUE]->Send_Ack = 0;
	queue[ROUTER_QUEUE]->cnt_node = 0;
}

// 라우터 인큐 함수
int Router_EnQueue( int pg_num )	{
	Queue_pt new_node = (Queue_pt)malloc(sizeof(struct Queue));	

	if( new_node == NULL )	// 할당받지못했을때
		PrintError("메모리 할당 에러");
	else	{
		// 할당 받았을때
		if( queue[ROUTER_QUEUE]->cnt_node > NODE_MAX_NUM )	{
			// 라우터 큐, 버퍼오버플로우 났을경우
			printf(">> [버퍼오버플로우] 라우터 인큐 실패 (pagenum: %d)\n", pg_num);
		}
		else	{
			// 라우터 큐, 버퍼가 넉넉할때
			new_node->right_link = new_node->left_link = NULL;
			new_node->pageNum = pg_num;
			
			// 메모리  정상적으로  할당 받았을경우
			if( queue[ROUTER_QUEUE]->right_link == NULL )	{
				// 첫노드 일 경우
				queue[ROUTER_QUEUE]->right_link = new_node;
			}
			else	{
				// 큐가 공백이 아니면
				Queue_pt move_temp;				// 맨끝 이동 노드용 move_temp
				move_temp = queue[ROUTER_QUEUE]->right_link;
				
				while( move_temp->right_link != NULL )	{
					// 맨끝 노드로이동
					move_temp = move_temp->right_link;
				}
				move_temp->right_link	= new_node;		// 마지막 노드 뒤에 새노드 추가
				new_node->left_link		= move_temp;	// 새노드랑 마지막 노드 연결
			}
			
			queue[ROUTER_QUEUE]->cnt_node++;			// 라우터큐의 노드카운트 증가
		
		}
	}
	return 0;
}

// 라우터 디큐 함수
int Router_DeQueue( int Send_Ack )	{
	Queue_pt	dequeue_temp;							// 맨 앞 노드 삭제용 dequeue_temp
	dequeue_temp = queue[ROUTER_QUEUE]->right_link;

	if(dequeue_temp == NULL)	{
		// 노드가 하나도 없을때
		return 0;
	}
	else	{
		// 노드가 한개이상일때
		if( Send_Ack >= dequeue_temp->pageNum)	{
			// 보낸 ACK보다 페이지넘버가 작거나 같은 에크는 다 디큐
			
			if( dequeue_temp->right_link == NULL )	{
				// 라우터큐에 노드가 한개일때			
				queue[ROUTER_QUEUE]->right_link = NULL;
			}
			else	{
				// 라우터큐에 노드가 두개 이상일때
				queue[ROUTER_QUEUE]->right_link = dequeue_temp->right_link;
				queue[ROUTER_QUEUE]->right_link->left_link = NULL;
			}

			dequeue_temp->right_link = dequeue_temp->left_link = NULL;	// 링크들 NULL로 초기화
			queue[ROUTER_QUEUE]->cnt_node--;							// 카운트 1개 감소
			Packet_EnQueue( dequeue_temp );								// 디큐된노드 패킷큐에 인큐
		}
		else	{
			// 보낸 ACK보다 큰 에크들은 그대로 냅둠
		}
	}
	
	return 0;
}

// 패킷 인큐 함수	
int Packet_EnQueue(Queue_pt head)	{
	if( queue[PACKET_QUEUE]->right_link == NULL )	{
		// 첫노드 일 경우
		queue[PACKET_QUEUE]->right_link = head;
	}
	else	{
		// 큐가 공백이 아니면
		Queue_pt temp;				// 맨끝 노드용 temp
		temp = queue[PACKET_QUEUE]->right_link;
		
		while(true)	{
			if( temp->pageNum == head->pageNum  )	{
				// queue[PACKET_QUEUE] 안의 노드의 페이지넘버와 인큐될 페이지넘버가 중복되면
				free(head);				// 인큐시키지않고 free 시켜줌
				
				return 0;
			}

			if( temp->right_link == NULL )	{
				// 마지막 노드 까지 이동			
				break;
			}
			else	{
				// 그외의 경우
				temp = temp->right_link;
			}
		}
		temp->right_link	= head;								// 마지막 노드 뒤에 새노드 추가
		head->left_link		= temp;								// 새노드랑 마지막 노드 연결
	}
	queue[PACKET_QUEUE]->Base_pageNum = (head->pageNum+1);		// 베이스 페이지넘버는 패킷큐에 인큐된 페이지넘버의 플러스 1 
	queue[PACKET_QUEUE]->cnt_node++;							// 패킷 큐의 노드카운트 증가

	return 0;
}


// 큐정보 출력함수
void PrintQueueInfo()	{
	Queue_pt packet_temp = queue[PACKET_QUEUE]->right_link;
	Queue_pt router_temp = queue[ROUTER_QUEUE]->right_link;

	printf("[ PACKET_QUEUE INFO ] ───────────────────────────── \n        ");
	if(queue[PACKET_QUEUE]->cnt_node == 0)	{
		// 노드가 0개이면
		printf("[ N U L L ] \n");
	}
	else if(queue[PACKET_QUEUE]->cnt_node < 8)	{
		//패킷큐 노드수가 8 개 미만이면
		while( packet_temp != NULL )	{
			printf("%d -> ", packet_temp->pageNum);
			packet_temp = packet_temp->right_link;
		}
		printf("[ E N D ] \n");

	}
	else	{
		//패킷큐 노드수가 8 개 이상이면
		printf("%d -> ", packet_temp->pageNum);
		while( packet_temp->right_link != NULL)	{
			// 마지막 노드 까지 이동
			packet_temp = packet_temp->right_link;
		}
		printf("     ................     -> %d -> ", packet_temp->pageNum);
		printf("[ E N D ] \n");

	}
	
	printf("\n");

	printf("[ ROUTER_QUEUE INFO ] ───────────────────────────── \n        ");
	if(queue[ROUTER_QUEUE]->cnt_node == 0)	{
		// 노드가 0개이면
		printf("[ N U L L ] \n");
	}
	else if(queue[ROUTER_QUEUE]->cnt_node < 8)	{
		// 라우터큐 노드수가 8 개 미만이면
		while( router_temp != NULL )	{
			printf("%d -> ",  router_temp->pageNum);
			router_temp = router_temp->right_link;
		}
		printf("[ E N D ] \n");
	}
	else	{
		// 라우터큐 노드수가 8 개 이상이면
		printf("%d -> ",  router_temp->pageNum);
		while( router_temp->right_link != NULL)	{
			// 마지막 노드 까지 이동
			router_temp = router_temp->right_link;
		}
		printf("     ................     -> %d -> ", router_temp->pageNum);
		printf("[ E N D ] \n");
	}
}

// 메뉴 출력 함수
void PrintMenu()	{
	printf("\n");
	printf("[ M E N U ] ────────────────────────────────── \n");
	printf("  (※ 숫자입력 => Send ACK) \n\n");
}