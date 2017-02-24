#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <process.h>
#include <time.h>

// 정의	----------------------------------------------------
#define SERV_IP			"127.0.0.1"		// IP 정의
#define SERV_PORT		7070			// 포트 정의
#define TIMER			5				// 타이머(초) 설정 
//---------------------- ↑서버 설정 -----------------------
#define BUF_ACK			0				// 버퍼에 ACK가 들어가있는 위치
#define	BUF_TIME		1				// 버퍼에 수신 타임이 들어가있는 위치
#define BUF_MAX_SIZE	2				// 버퍼사이즈 100
//-------------------- ↑buf 관련 정의 ---------------------
#define PACKET_QUEUE	0				// 패킷 큐
#define ROUTER_QUEUE	1				// 라우터 큐
#define	QUEUE_MAX_NUM	2				// 큐 갯수
#define NODE_MAX_NUM	10				// 큐의 크기
//-------------------- ↑큐 관련 정의 ----------------------
#define DEFAULT_TYPE	'0'				// 기본
#define PACKET_TYPE		'1'				// 패킷만들기
#define ROUTER_TYPE		'2'				// 라우터
//---------------------- ↑타입 정의 -----------------------
#define	THRESHOLD		10				// 임계점
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
	int Send_pageNum;		// 보낸 마지막 노드의 pageNum
	int start_time;			// 시작 시간
	int end_time;			// ACK 시간

	Queue_pt right_link;	
};
//----------------------------------------------------------



// 함수 정의 -----------------------------------------------
void PrintError( char * );								// 에러메세지 출력
int Packet_EnQueue( int );								// 패킷큐 인큐
int Packet_DeQueue( void );								// 패킷큐 디큐	-> 라우터 인큐진행
int Router_EnQueue( Queue_pt );							// 라우터 인큐
int Router_DeQueue( void );								// 라우터 디큐
int Router_PeekQueue( Queue_pt );						// 라우터 피큐
void Initialize_ALL();									// 초기화 함수
void PrintIndex();										// 인덱스 출력함수
void PrintQueueInfo();									// 큐정보 출력함수
void PrintMenu();										// 메뉴 출력 함수

unsigned WINAPI MakePkt( void* );
unsigned WINAPI SendPkt( void* );
unsigned WINAPI RcvPkt( void* );
//----------------------------------------------------------


// 전역변수 정의 -------------------------------------------
SOCKET sHandle;							// 서버 소켓 핸들
SOCKET cHandle;							// 클라이언트 소켓 핸들

SOCKADDR_IN	sAddr,						// 소켓구조체 서버주소
			cAddr;						// 소켓구조체 클라이언트

HANDLE	rqSema;							// 라우터큐 세마포어

Queue_Head_pt		queue[ QUEUE_MAX_NUM ];

int buf[BUF_MAX_SIZE];					// 클라이언트와 주고받기위한 buf배열
int type;								// 타입값 설정
int Make_Page_Num,						// 만들어질 페이지넘버
	Ack_Cnt,							// 에크카운트
	Window_Size;						// 윈도우 사이즈
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


	if ( bind( sHandle, (SOCKADDR*) &sAddr, sizeof(sAddr) ) == SOCKET_ERROR )
		PrintError("bind() 에러");
	if( listen( sHandle, 5 ) == SOCKET_ERROR )
		PrintError("listen() 에러");
	

	rqSema	= CreateSemaphore(NULL, 1, 1, NULL);			// 세마포어 생성
	

	Initialize_ALL();
	int cAddrSize;
	while( !(cHandle)  )	{
		cAddrSize = sizeof(cAddr);
		
		cHandle = accept( sHandle, (SOCKADDR*)&cAddr,&cAddrSize );
		
		printf("Connected client IP: %s \n", inet_ntoa(cAddr.sin_addr));
	}
	//---------------------------------------------------------------
	HANDLE		MakeThread, SendThread, RcvThread;
	//------------------ ↑ 쓰레드 핸들 선언 ------------------------
	MakeThread=
		(HANDLE)_beginthreadex(NULL, 0, MakePkt, NULL, 0, NULL);
	SendThread=
		(HANDLE)_beginthreadex(NULL, 0, SendPkt, NULL, 0, NULL);
	RcvThread=
		(HANDLE)_beginthreadex(NULL, 0, RcvPkt, NULL, 0, NULL);
	//--------------------- ↑ 쓰레드 생성 --------------------------

	printf("[서버]연결 완료\n");
	Sleep(2 * 1000);
	PrintIndex();
	PrintMenu(); 



	WaitForSingleObject( MakeThread, INFINITE );
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

// 초기화 함수
void Initialize_ALL()	{
	for(int i=0; i<QUEUE_MAX_NUM; i++)	{
		// 패킷큐, 라우터큐 초기화
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

// 인큐 함수
int Packet_EnQueue()	{
	Queue_pt new_node = (Queue_pt)malloc(sizeof(struct Queue));	

	if( new_node == NULL )
		PrintError("메모리 할당 에러");
	else	{
		new_node->right_link = new_node->left_link = NULL;
		new_node->pageNum = Make_Page_Num;
		
		// 메모리  정상적으로  할당 받았을경우
		if( queue[PACKET_QUEUE]->right_link == NULL )	{
			// 첫노드 일 경우
			queue[PACKET_QUEUE]->right_link = new_node;
		}
		else	{
			// 큐가 공백이 아니면
			Queue_pt move_temp;				// 맨끝 이동 노드용 move_temp
			move_temp = queue[PACKET_QUEUE]->right_link;
			
			while( move_temp->right_link != NULL )	{
				// 맨끝 노드로이동
				move_temp = move_temp->right_link;
			}
			move_temp->right_link	= new_node;		// 마지막 노드 뒤에 새노드 추가
			new_node->left_link		= move_temp;	// 새노드랑 마지막 노드 연결
		}

		Make_Page_Num++;							// 만들어질 페이지넘버 증가
		queue[PACKET_QUEUE]->cnt_node++;			// 패킷큐의 노드카운트 증가
	}

	return 0;
}

// 패킷 디큐 함수
int Packet_DeQueue()	{
	if( queue[ROUTER_QUEUE]->cnt_node >= NODE_MAX_NUM )	{
		// 라우터 최대 노드수 초과했을때 오버플로우
		return 0;
	}
	else	{
		// 최대노드수 초과하지않을때
		Queue_pt	dequeue_temp;			// 맨 앞 노드 삭제용 dequeue_temp
		dequeue_temp = queue[PACKET_QUEUE]->right_link;

		if( dequeue_temp->right_link == NULL )	{
			// 노드가 한개뿐일때
			queue[PACKET_QUEUE]->right_link = NULL;
		}
		else	{
			// 노드가 두개이상일때
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

// 라우터 인큐 함수	
int Router_EnQueue(Queue_pt head)	{
	if( queue[ROUTER_QUEUE]->right_link == NULL )	{
		// 첫노드 일 경우
		queue[ROUTER_QUEUE]->right_link = head;
	}
	else	{
		// 큐가 공백이 아니면
		Queue_pt temp;				// 맨끝 노드용 temp
		temp = queue[ROUTER_QUEUE]->right_link;
		
		while( temp->right_link != NULL )	{
			// 맨끝 노드로이동
			temp = temp->right_link;
		}
		temp->right_link	= head;		// 마지막 노드 뒤에 새노드 추가
		head->left_link		= temp;		// 새노드랑 마지막 노드 연결
	}

	queue[ROUTER_QUEUE]->cnt_node++;												// 라우터 큐의 노드카운트 증가

	return 0;
}


// 라우터 피크 큐 함수
int Router_PeekQueue(Queue_pt tmp)	{
	if( tmp == NULL )	// 노드수가 1개도 없을때
		return 0;
	else	{
		// 노드수가 1개 이상일때
		buf[BUF_ACK] = tmp->pageNum;
		queue[ROUTER_QUEUE]->Send_pageNum = tmp->pageNum;			// 마지막 노드 페이지넘버 기록
	
		tmp->start_time = (int)time(NULL);							// send 되어질때의 시간을 기록
		queue[ROUTER_QUEUE]->start_time = tmp->start_time;			// 보낸 시간 입력
		queue[ROUTER_QUEUE]->end_time	= tmp->start_time;			// 동기화
	}
	
	return 0;
}
 
// ACK 체크 함수
int Ack_Check( int ack )	{
	if( ack >= queue[ROUTER_QUEUE]->Base_pageNum && ack <= queue[ROUTER_QUEUE]->Send_pageNum  )	{
		// 받은 ack가 보낸 페이지넘버의 첫번째랑 마지막 사이에 있을때
		if( queue[ROUTER_QUEUE]->end_time - queue[ROUTER_QUEUE]->start_time > TIMER )	{
			// ack를 받았을때의 시간이 계산된 끝날 시간보다 클때, 즉 타임아웃일때
			printf(">>> [타임아웃]페이지 넘버 : %d \n", ack);

			Window_Size = 1;	// 타임아웃은 심각한문제이므로 윈도우사이즈 1로 조정
		}
		else	{
			// 정상적일때, 즉 타임아웃X
			for(int i=queue[ROUTER_QUEUE]->Base_pageNum; i<=ack;i++)
				Router_DeQueue();
			WaitForSingleObject(rqSema, INFINITE);
			queue[ROUTER_QUEUE]->Base_pageNum = (ack+1);
			ReleaseSemaphore( rqSema, 1, NULL );
		

			if( Window_Size < THRESHOLD )	{
				// 윈도우 사이즈가 임계치보다 작으면
				Window_Size *= 2;	// 윈도우 사이즈 지수증가
			}
			else	{
				// 윈도우 사이즈가 임계치보다 크면
				Window_Size++;		// 윈도우 사이즈 선형증가
			}

			PrintIndex();
			PrintQueueInfo();
			PrintMenu(); 
			printf("[ RCV_INFO ] ────────────────────────── \n");
			printf(">>> Rcv pkt : %d \n", ack);
			printf(">>> [디큐완료]윈도우사이즈 증가(현재윈도우사이즈:%d)\n", Window_Size);
			
		}
	}
	else if( ack < queue[ROUTER_QUEUE]->Base_pageNum )	{
		// 보낸 페이지번호보다 받은 ACK 작을때, 즉, 이미 넘어간 페이지에대해 ACK를 받았을때
		Ack_Cnt++;		// 에크 카운트 증가
	}
	else	{
		// 전혀 상관없는 ack가 왔을때
		printf(">>> [유효하지않은 ACK] ACK num : %d \n", ack);
	}
	

	if( Ack_Cnt >= 3 )	{
		// 중복ACK가 3번이상 발생했을때
		if(Window_Size == 1)	{
			// 윈도우 사이즈 1일경우
			Window_Size = 1;	// 그대로 1로 냅둠. 2로나누면 0되기때문
		}
		else	{
			// 윈도우 사이즈 2이상일 경우
			Window_Size /= 2;	// 윈도우 사이즈 지수감소
		}
		printf(">>> [중복ACK] pagenum : %d \n", ack);
		Ack_Cnt = 0;			// 다시 0으로 초기화
	}

	return 0;
}

// 라우터 디큐 함수
int Router_DeQueue()	{
	// 맨앞 노드 디큐
	Queue_pt delete_temp;							// 맨앞 노드 삭제용 delete_temp
	delete_temp = queue[ROUTER_QUEUE]->right_link;
	
	if(delete_temp == NULL)	{
		// 라우터큐에 노드가 하나도없을때
		return 0;
	}
	else if( delete_temp->right_link == NULL )	{
		// 라우터큐에 노드가 하나있을때
		queue[ROUTER_QUEUE]->right_link = NULL;
	}
	else	{
		// 라우터큐에 노드가 두개 이상있을때
		queue[ROUTER_QUEUE]->right_link = delete_temp->right_link;
		queue[ROUTER_QUEUE]->right_link->left_link = NULL;
	}


	queue[ROUTER_QUEUE]->cnt_node--;

	free(delete_temp);
	
	return 0;
}

// 패킷 만드는 쓰레드
unsigned WINAPI MakePkt(void * arg)	{
	while(true)	{
		scanf("%c", &type);
		fflush(stdin);

		if( type == PACKET_TYPE )	{
			Packet_EnQueue();	// 패킷큐에 인큐
			Packet_DeQueue();	// 패킷큐를 디큐 후, 라우터큐에 인큐
		
			PrintIndex();
			PrintQueueInfo();
			PrintMenu(); 
		}
	}
	return 0;
}

// 패킷 보내는 쓰레드
unsigned WINAPI SendPkt(void * arg)	{
	Queue_pt tmp = (Queue_pt)malloc(sizeof(struct Queue));	

	
	while(true) {
	
		while( type == ROUTER_TYPE )	{
			WaitForSingleObject(rqSema, INFINITE);
			tmp = queue[ROUTER_QUEUE]->right_link;
		
			PrintIndex();
			PrintQueueInfo();
			PrintMenu(); 
			printf("[ SEND_INFO ] ───────────────────────── \n");
			printf(">>> 현재 윈도우 사이즈 : %d \n", Window_Size);
			
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

// 패킷 받는 쓰레드
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
		printf("[ RCV_INFO ] ────────────────────────── \n");
		printf(">>> Rcv pkt : %d \n", buf[BUF_ACK])	;	
		Ack_Check( buf[BUF_ACK] );
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
	printf("┃                                              Server                      ┃\n");
	printf("┃                                                                          ┃\n");
	printf("┃                                                                          ┃\n");
	printf("┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛\n\n");

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
	printf("  1. 패킷 만들기 & 라우터인큐 \n");
	printf("  2. 패킷 전송 \n\n");
}