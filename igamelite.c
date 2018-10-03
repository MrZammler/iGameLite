/*
*	igamelite.c test
*/

#include <exec/types.h>
//#include <exec/memory.h>
//#include <exec/devices.h>
//#include <devices/keymap.h>
//#include <graphics/gfx.h>
//#include <graphics/copper.h>
//#include <graphics/GfxBase.h>
#include <graphics/rastport.h>
#include <intuition/intuition.h>
//#include <intuition/IntuitionBase.h>
#include <dos/dos.h>
#include <exec/exec.h>
#include <stdio.h>
#include <string.h>

#include <devices/gameport.h>
#include <devices/inputevent.h>

extern struct IOStdReq *CreateStdIO();
extern struct MsgPort *CreatePort();

#define JOY_X_DELTA (1)
#define JOY_Y_DELTA (1)
#define TIMEOUT_SECONDS (10)

#define WIDTH 640
#define HEIGHT 256
#define BUFSIZE 1000

#define RP s->RastPort

void TypeWriterText(char *text);
void DrawGames(int NewPointer);
void RunGame();

int CurGame = 0;

struct GfxBase *GfxBase;
struct IntuitionBase *IntuitionBase;
extern struct ExecBase *SysBase;

struct GamePortTrigger   joytrigger;
struct IOStdReq         *game_io_msg;
struct MsgPort          *game_msg_port;

struct NewScreen NewScreen =
		{
			0,0,WIDTH,HEIGHT,2,
			1,0,
			HIRES,
			CUSTOMSCREEN,
			NULL,
			"",
			NULL,NULL
		};

struct Screen *OpenScreen();
struct Screen *s;

struct TextAttr myAttr = {"topaz.font",8,0,0};
struct TextFont *tf;

/*-----------------------------------------------------------------------
** print out information on the event received.
*/
BOOL check_move(struct InputEvent *game_event)
{
WORD xmove, ymove;
BOOL timeout=FALSE;

xmove = game_event->ie_X;
ymove = game_event->ie_Y;

if (xmove == 1)
    {
	//if (ymove == 1) printf("RIGHT DOWN\n");
	//else if (ymove == 0) printf("RIGHT\n");
	//else if (ymove ==-1) printf("RIGHT UP\n");
	//else printf("UNKNOWN Y\n");

	printf("Next page\n");

    }
else if (xmove ==-1)
    {
	//if (ymove == 1) printf("LEFT DOWN\n");
	//else if (ymove == 0) printf("LEFT\n");
	//else if (ymove ==-1) printf("LEFT UP\n");
	//else printf("UNKNOWN Y\n");

	printf("Previous page\n");

    }
else if (xmove == 0)
    {
	if (ymove == 1) { DrawGames(CurGame+1); }
    /* note that 0,0 can be a timeout, or a direction release. */
    else if (ymove == 0)
        {
        if (game_event->ie_TimeStamp.tv_secs >=
                        (UWORD)(SysBase->VBlankFrequency)*TIMEOUT_SECONDS)
            {
			//printf("TIMEOUT\n");
            timeout=TRUE;
            }
		//else printf("RELEASE\n");
        }
	else if (ymove ==-1) DrawGames(CurGame-1);
	//else printf("UNKNOWN Y\n");
    }
else
    {
    printf("UNKNOWN X ");
    if (ymove == 1) printf("unknown action\n");
    else if (ymove == 0) printf("unknown action\n");
    else if (ymove ==-1) printf("unknown action\n");
    else printf("UNKNOWN Y\n");
    }

return(timeout);

}

/*-----------------------------------------------------------------------
** send a request to the gameport to read an event.
*/
VOID send_read_request( struct InputEvent *game_event,
                        struct IOStdReq *game_io_msg)
{
game_io_msg->io_Command = GPD_READEVENT;
game_io_msg->io_Flags   = 0;
game_io_msg->io_Data    = (APTR)game_event;
game_io_msg->io_Length  = sizeof(struct InputEvent);
SendIO(game_io_msg);  /* Asynchronous - message will return later */
}

/*-----------------------------------------------------------------------
** simple loop to process gameport events.
*/
VOID processEvents( struct IOStdReq *game_io_msg,
                    struct MsgPort  *game_msg_port)
{
BOOL timeout;
SHORT timeouts;
SHORT button_count;
BOOL  not_finished;
struct InputEvent game_event;   /* where input event will be stored */

/* From now on, just read input events into the event buffer,
** one at a time.  READEVENT waits for the preset conditions.
*/
timeouts = 0;
button_count = 0;
not_finished = TRUE;

while ((timeouts < 6) && (not_finished))
    {
    /* Send the read request */
    send_read_request(&game_event,game_io_msg);

    /* Wait for joystick action */
    Wait(1L << game_msg_port->mp_SigBit);
	while (GetMsg(game_msg_port))
        {
        timeout=FALSE;
        switch(game_event.ie_Code)
            {
            case IECODE_LBUTTON:
				//printf(" FIRE BUTTON PRESSED \n");
				RunGame();
				break;

            case (IECODE_LBUTTON | IECODE_UP_PREFIX):
				//printf(" FIRE BUTTON RELEASED \n");
                if (3 == ++button_count)
                    not_finished = FALSE;
                break;

            case IECODE_RBUTTON:
				//printf(" ALT BUTTON PRESSED \n");
                button_count = 0;
                break;

            case (IECODE_RBUTTON | IECODE_UP_PREFIX):
				//printf(" ALT BUTTON RELEASED \n");
                button_count = 0;
                break;

			case IECODE_NOBUTTON:
				/* Check for change in position */
				timeout = check_move(&game_event);
				button_count = 0;
				break;

            default:
                break;
            }

		//if (timeout)
		//	  timeouts++;
		//else
		//	  timeouts=0;
        }
    }
}

/*-----------------------------------------------------------------------
** allocate the controller if it is available.
** you allocate the controller by setting its type to something
** other than GPCT_NOCONTROLLER.  Before you allocate the thing
** you need to check if anyone else is using it (it is free if
** it is set to GPCT_NOCONTROLLER).
*/
BOOL set_controller_type(BYTE type, struct IOStdReq *game_io_msg)
{
BOOL success = FALSE;
BYTE controller_type = 0;

/* begin critical section
** we need to be sure that between the time we check that the controller
** is available and the time we allocate it, no one else steals it.
*/
Forbid();

game_io_msg->io_Command = GPD_ASKCTYPE;    /* inquire current status */
game_io_msg->io_Flags   = IOF_QUICK;
game_io_msg->io_Data    = (APTR)&controller_type; /* put answer in here */
game_io_msg->io_Length  = 1;
DoIO(game_io_msg);

/* No one is using this device unit, let's claim it */
if (controller_type == GPCT_NOCONTROLLER)
    {
    game_io_msg->io_Command = GPD_SETCTYPE;
    game_io_msg->io_Flags   = IOF_QUICK;
    game_io_msg->io_Data    = (APTR)&type;
    game_io_msg->io_Length  = 1;
    DoIO( game_io_msg);
    success = TRUE;
    }

Permit(); /* critical section end */
return(success);
}

/*-----------------------------------------------------------------------
** tell the gameport when to trigger.
*/
VOID set_trigger_conditions(struct GamePortTrigger *gpt,
                            struct IOStdReq *game_io_msg)
{
/* trigger on all joystick key transitions */
gpt->gpt_Keys   = GPTF_UPKEYS | GPTF_DOWNKEYS;
gpt->gpt_XDelta = JOY_X_DELTA;
gpt->gpt_YDelta = JOY_Y_DELTA;
/* timeout trigger every TIMEOUT_SECONDS second(s) */
gpt->gpt_Timeout = (UWORD)(SysBase->VBlankFrequency) * TIMEOUT_SECONDS;

game_io_msg->io_Command = GPD_SETTRIGGER;
game_io_msg->io_Flags   = IOF_QUICK;
game_io_msg->io_Data    = (APTR)gpt;
game_io_msg->io_Length  = (LONG)sizeof(struct GamePortTrigger);
DoIO(game_io_msg);
}

/*-----------------------------------------------------------------------
** clear the buffer.  do this before you begin to be sure you
** start in a known state.
*/
VOID flush_buffer(struct IOStdReq *game_io_msg)
{
game_io_msg->io_Command = CMD_CLEAR;
game_io_msg->io_Flags   = IOF_QUICK;
game_io_msg->io_Data    = NULL;
game_io_msg->io_Length  = 0;
DoIO(game_io_msg);
}

/*-----------------------------------------------------------------------
** free the unit by setting its type back to GPCT_NOCONTROLLER.
*/
VOID free_gp_unit(struct IOStdReq *game_io_msg)
{
BYTE type = GPCT_NOCONTROLLER;

game_io_msg->io_Command = GPD_SETCTYPE;
game_io_msg->io_Flags   = IOF_QUICK;
game_io_msg->io_Data    = (APTR)&type;
game_io_msg->io_Length  = 1;
DoIO(game_io_msg);
}

int main()
{
	int i, chip;
	char chrTemp[100];


	printf("iGameLite v0.1 PAL (c) 2006 Emmanuel Vasilakis\n");

	GfxBase = (struct GfxBase *)OpenLibrary("graphics.library",0);
	IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library",0);

	chip = AvailMem(MEMF_CHIP);
	printf("Chip: %d\n", chip);
	//printf("here");

	//if ((IntuitionBase = (struct IntuitionBase *)

	s = OpenScreen (&NewScreen);

	//font
	tf = (struct TextFont *)OpenFont(&myAttr);

	//black screen
	SetRGB4(&s->ViewPort,0,0,0,0);

	//clear screen
	SetRast(&RP,0);
	SetFont(&RP,tf);

    //help settle the screen
	Delay(50);

	SetAPen(&RP,3);

	Move(&RP,20,250);
	chip = AvailMem(MEMF_CHIP);
	sprintf(chrTemp, "Chip: %d", chip);
	Text(&RP, chrTemp, strlen(chrTemp));

	Move(&RP,0,15);
	TypeWriterText("Welcome to iGameLite");

	//draw a line
	for (i=0;i<640;i++){
		WritePixel(&RP,i,20);
		//Delay(1);
	}

	DrawGames(1);

	/* Create port for gameport device communications */
	if (game_msg_port = CreatePort("RKM_game_port",0))
    {
		/* Create message block for device IO */
		if (game_io_msg = (struct IOStdReq *)
                      CreateExtIO(game_msg_port,sizeof(*game_io_msg)))
		{
			game_io_msg->io_Message.mn_Node.ln_Type = NT_UNKNOWN;

			/* Open the right/back (unit 1, number 2) gameport.device unit */
			if (!OpenDevice("gameport.device",1,game_io_msg,0))
			{
				/* Set controller type to joystick */
				if (set_controller_type(GPCT_ABSJOYSTICK,game_io_msg))
                {
					/* Specify the trigger conditions */
					set_trigger_conditions(&joytrigger,game_io_msg);

					/* Clear device buffer to start from a known state.
					** There might still be events left
					*/
					flush_buffer(game_io_msg);

					processEvents(game_io_msg,game_msg_port);

					/* Free gameport unit so other applications can use it ! */
					free_gp_unit(game_io_msg);
                }
				CloseDevice(game_io_msg);
            }
			DeleteExtIO(game_io_msg);
		}
		DeletePort(game_msg_port);
    }

	//delay a bit
	//Delay(250);

	
}

void TypeWriterText(char *text)
{
	int i,k;
	char letter[2];

	for (i=0;i<strlen(text);i++)
	{
		sprintf(letter, "%c", text[i]);
		Text(&RP,letter,1);
		//for (k=0;k<=10000000;k++) {}
		Delay(4);
	}


}

void DrawGames(int NewPointer)
{
	char FileLine[300], Title[100], Title2[100], selection[5];
	int i,numbering=0,k, mem;
	FILE *fpgames;

    fpgames=fopen("SYS:s/gameslist", "r");

    if (!fpgames){
		printf("Could not open SYS:s/gameslist...\n");
		exit(0);
    }else{
		Move(&RP,20,39);
		ClearScreen(&RP);
        do{
            if (fgets (FileLine, sizeof(FileLine), fpgames)==NULL) { break; }
            FileLine[strlen(FileLine)-1]='\0';

            if (strlen(FileLine)==0) continue;
			if (!strncmp(FileLine, "path=", 5)) continue;

			numbering++;

            //title first
			/* skip the title= part */
			k=0;
			for (i=6;i<=strlen(FileLine);i++){
				Title[k]=FileLine[i];
				k++;
			}

			if (NewPointer == numbering)
				sprintf(Title2, "-> %s", Title);
			else
				sprintf(Title2, "   %s", Title);

			Move(&RP,20,30+(numbering*9));
			Text(&RP, Title2, strlen(Title2));

			CurGame = NewPointer;

			//if (numbering%GAMES_PER_PAGE==0) break;

			//path next
			//fgets (FileLine, sizeof(FileLine), fpgames);

			//FileLine[strlen(FileLine)-1]='\0';

			//printf("Path: [%s]\n", FileLine);

        }while(1);

		fclose(fpgames);
    }

}

void RunGame()
{
	int chip;
    char FileLine[300], Path[200], selection[5], naked_path[100], slave[50], exec[50];
	int i,numbering=0,k,z, success;
	BPTR newlock, oldlock, origlock, lock;
    FILE *fpgames;

    CloseScreen(s);

	//chip = AvailMem(MEMF_CHIP);
	//printf("Chip: %d\n", chip);

	free_gp_unit(game_io_msg);
	CloseDevice(game_io_msg);
	DeleteExtIO(game_io_msg);
	DeletePort(game_msg_port);

    fpgames=fopen("SYS:s/gameslist", "r");

    if (!fpgames){
		printf("Could not open SYS:s/gameslist...\n");
		exit(0);
    }else{
        do{
            if (fgets (FileLine, sizeof(FileLine), fpgames)==NULL) { break; }
            FileLine[strlen(FileLine)-1]='\0';

            if (strlen(FileLine)==0) continue;
			if (!strncmp(FileLine, "title=", 6)) continue;

			numbering++;

			if (numbering==CurGame){
				/* skip the path= part */
				k=0;
				for (i=5;i<=strlen(FileLine);i++){
					Path[k]=FileLine[i];
					k++;
				}

				//printf("Orig Path: [%s]\n", Path);

                /* strip the path from the slave file and get the rest */
				for (i=strlen(Path)-1;i>=0;i--){
					if (Path[i]=='/')
					break;
				}
				for (k=0;k<=i-1;k++)
					naked_path[k]=Path[k];
				naked_path[k]='\0';

				for (k=i+1;k<=strlen(Path)-1;k++) {
					slave[z]=Path[k];
					z++;
				}
				slave[z]='\0';

				for (i=0;i<=strlen(slave)-1;i++) slave[i] = tolower(slave[i]);

				//printf("Naked path: [%s] - Slave [%s]\n", naked_path, slave);

                oldlock = Lock("PROGDIR:", ACCESS_READ);

				lock = Lock(naked_path, ACCESS_READ);
				CurrentDir(lock);

				sprintf(exec, "SYS:c/whdload %s", slave);

				//printf("Executing: %s\n", exec);

                success = Execute(exec,0,0);

			}
        }while(1);
    }

    CurrentDir(oldlock);
	fclose(fpgames);

	exit(0);
}
