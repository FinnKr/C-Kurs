/*	tb_change_cell(int koorX, int koorY, char Zeichen, int FarbeZeichen, int FarbeBackGround)
	--> damit manipuliert man Pixel

	tb_present();
	--> aktuallisiert die Termbox für den Anwender

	tb_poll_event()
		--> Wartet auf ein Event (Mouseclick, Keyboard)
*/
#include <stdio.h>
#include <string.h>
#include "termbox.h"
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>

// DEFINES
//#define maxBombCount 6   // Bomben_pro_Spieler * Spieler 
//#define RADIUS 6

// wir gehen davon aus dass 200 = 2s ist
//#define EXPLODETIME 3
//#define DMGTIME 2

		// Farben
#define RED "\033[31m"
#define BOLD "\033[1m"
#define RESET "\033[0m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"



//STRUCTS
//map speichert Infos zur genutzten Map
struct map{
	int hoehe, breite;
	uint16_t vg, hg;
	int spawnAx, spawnAy;
	int spawnBx, spawnBy;
	char *ptr;
};
// ein 'Pixel'
typedef struct{
	int x;
	int y;
	unsigned char ch;
}key;	
//Player
typedef struct{
	int x;				// Position auf der map
	int y;
	char ch;
	int bombs;			// Anzahl der Bomben im Inventar
	int isDead;			// ob der Spieler noch lebt
	int isActive;		// ob der Spieler am Spiel teilnimmt bzw. existiert
}player;

typedef struct{
	int x;				// Position auf der map
	int y;
	int radius;			// Explosionsradius
	int timer;			// Wie viele ticks bis zur Explosion uebrig sind
	int isActive;		// ob die Bombe existiert
}bomb;


// GLOBALE VARIABLEN
static struct map *map_ptr;
int zustand = 0, fs;
FILE *log = NULL;
int winner = -1;
int EXPLODETIME = 3;
int DMGTIME = 2;
int maxBombCount = 6;
int RADIUS = 6;

// FUNKTIONEN deklarationen
void show_anim(int);
int startmenu(void);

	// Termbox
int createTermbox();
int move(uint16_t button, player *p);
int check(int, player *);
int move2(uint32_t button, player *p);
int check2(int direct, player *p2);

void read_map(struct map *, char *);
uint16_t hex_to_int(char);
char getc_arr(int, int);
void show_endanim();
void *tick(void *);
char *getLink();
int init();
int fullscreen(int, char **);
void generate_map(int, int);
void start_generating();
void write_char(int, int, char);
		// Bomben
void plantBomb(player *p, bomb *bomb_Array);
int findFreeBombSlot(bomb *bomb_Array);
void tickBombs(int *dmg_Array, bomb *bomb_Array);
void explosion(int *dmg_Array, bomb *bomb_Array);
int directionNonSolid(int x, int y, char direction);
void read_config();
void read_var(FILE *, int *);

int main(int argc, char **argv){
	fs = fullscreen(argc, argv);
	if (fs){
		system("wmctrl -r ':ACTIVE:' -b toggle,fullscreen");//Macht Window zum Fullscreen oder beendet ihn
		usleep(10000);//warten das in Vollbild gewechselt wurde
	}
	if (init())//fürt benötigte Code aus, der keine Rückgabewerte hat
		return 0;
	
	// res speichert den eigentlichen return-Wert
	int res;
	res = createTermbox();
	zustand = 3;//beendet den Loop in tick() und damit den Thread
	show_endanim();
	if(winner == 0)
		// Unentschieden
		printf(BOLD YELLOW "\nDrawn game!\n\n" RESET);
	else if(winner == 1 || winner == 2)
		// Einer gewinnt
		printf(BOLD GREEN "\nPlayer %d wins!\n\n" RESET, winner);
	else 
		// Spiel abgebrochen
		printf(BOLD RED "\nGame canceled!\n\n" RESET);

	if (fs){
		system("wmctrl -r ':ACTIVE:' -b toggle,fullscreen");//Macht Window zum Fullscreen oder beendet ihn
	}
	free(map_ptr->ptr);
	free(map_ptr);
	return(res);
}


void *tick(void *inp){
	while (zustand != 3){
		//do something
		usleep(1000*10);
	}
	return inp;
}


void generate_map(int breite, int hoehe){
	map_ptr->hoehe = hoehe;
	map_ptr->breite = breite;
	map_ptr->ptr = malloc(breite*hoehe);
	map_ptr->vg = 0;
	map_ptr->hg = 0;
	//leere Map erstellen:
	for (int i = 0; i < hoehe; i++){
		for (int j = 0; j < breite; j++){
			if (i == 0 || i == hoehe-1){
				if (j == 0 || j == breite -1){
					*(map_ptr->ptr) = '+';
				}else{
					*(map_ptr->ptr) = '-';
				}
			}else{
				if (j == 0 || j == breite -1){
					*(map_ptr->ptr) = '|';
				}else{
					*(map_ptr->ptr) = ' ';
				}
			}
			(map_ptr->ptr)++;
		}
	}
	(map_ptr->ptr) -= breite*hoehe;
	//Muster erzeugen:
	int elm_count = hoehe + breite - 6;
	srand(time(NULL));
	int rh = 0, rb = 0;
	for (int i = 0; i < elm_count; i++){
		rh = (rand() % (hoehe-2)) + 1;
		rb = (rand() % (breite-2)) + 1;
		if (getc_arr(rb, rh) == '#'){
			i--;
		}else{
			write_char(rb, rh, '#');
		}
	}
	//Spawns generieren:
	for (int spawns = 2; spawns > 0;){
		rh = (rand() % (hoehe-2)) + 1;
		rb = (rand() % (breite-2)) + 1;
		if (getc_arr(rb, rh) != '#'){
			if (spawns == 2){
				map_ptr->spawnAx = rb;
				map_ptr->spawnAy = rh;
				spawns--;
			}else if (spawns == 1){
				map_ptr->spawnBx = rb;
				map_ptr->spawnBy = rh;
				spawns--;
			}
		}
	}
}

void start_generating(){
	int b = 0, h = 0;
	char c = 'n';
	printf("Please enter a width:\n> ");
	c = getchar();
	while (c != '\n'){
		b *= 10;
		b += c - '0';
		c = getchar();
	}
	if (b < 4 || b > 200){
		printf("Bad size, using default.(64)\n");
		b = 64;
	}
	printf("Please enter a height:\n> ");
	c = getchar();
	while (c != '\n'){
		h *= 10;
		h += c - '0';
		c = getchar();
	}
	if (h < 4 || h > 100){
		printf("Bad size, using default.(32)\n");
		h = 32;
		sleep(2);
	}
	c = 'n';
	while (c == 'n'){
		generate_map(b, h);

		printf("Height: %d, Width: %d\nSpawn A: (%d, %d)\nSpawn B: (%d, %d)\n", map_ptr->hoehe, map_ptr->breite, map_ptr->spawnAx, map_ptr->spawnAy, map_ptr->spawnBx, map_ptr->spawnBy);
		for (int i = 0; i < h; i++){
			for (int j = 0; j < b; j++){
				printf("%c", *(map_ptr->ptr));
				(map_ptr->ptr)++;
			}
			printf("\n");
		}
		(map_ptr->ptr) -= h*b;
		printf("\n Play this map? [y] for yes, [n] for no (and generate new map)\n> ");
		c = getchar();
		while (c != 'n' && c != 'j' && c !='y'){
			if (c == '\n')
				printf("> ");
			c = getchar();
		}
	}
}

void write_char(int x, int y, char c){
	int pos = map_ptr->breite * y + x;
	map_ptr->ptr += pos;
	*(map_ptr->ptr) = c;
	map_ptr->ptr -= pos;
}

int fullscreen(int argc, char **argv){
	if (argc > 1){
		argv++;
		char *tmp = malloc(3);
		strncpy(tmp, *argv, 2);
		argv--;
		tmp += 2;
		*tmp = '\0';
		tmp -= 2;
		if (strcmp(tmp, "-f") == 0 || strcmp(tmp, "-v") == 0){
			free(tmp);
			return 1;
		}
		free(tmp);
	}
	return 0;
}

char *getLink(){
	int id = 0;
	struct txt{
		int id;
		char *name;
		int length;
		struct txt *next;
	};
	struct txt *maps = malloc(sizeof (struct txt)), *anfang;
	anfang = maps;

	system("clear");
	show_anim(0);

	DIR *dir;
	struct dirent *dirptr;
	if((dir=opendir("./Maps")) != NULL){
		while((dirptr = readdir(dir)) != NULL){
			if ((*dirptr).d_name[0] != '.'){
				id++;
				maps->id = id;
				maps->length = strlen((*dirptr).d_name);
				maps->name = malloc(maps->length+1);
				strcpy(maps->name, (*dirptr).d_name);
				maps->next = malloc(sizeof (struct txt));
				maps = maps->next;
			}
		}
		
		maps = anfang;
		// printf("Welche Map soll geladen werden?\n\t-[0]Map generieren\n");
		printf(BOLD YELLOW "Choose map:\n" RESET);
		printf("\t-[0]generate random map\n");
		for (int i = 0; i < id; i++){
			char *tmp = malloc(maps->length - 3);
			strncpy(tmp, maps->name, maps->length - 4);
			printf("\t-[%d]%s\n", maps->id, tmp);
			free(tmp);
			maps = maps->next;
		}
		maps = anfang;
		char c;
		int num = -1;
		while (1){
			num = -1;
			printf("> ");
			c = getchar();
			while (c >= '0' && c <= '9'){
				if (num == -1){
					num = 0;
				}
				num *= 10;
				num += c-'0';
				c = getchar();
			}
			while (c != '\n'){
				c = getchar();
			}
			if (num == 0){
				maps = anfang;
				struct txt *tmp = maps;
				for (;id > 0; id--){
					tmp = maps->next;
					free(maps->name);
					free(maps);
					maps = tmp;
				}
				return "-generate-";
			}else if (id >= num && num != -1){
				num--;
				for (; num > 0; num--){
					maps = maps->next;
				}
				char *ret = malloc(7 + maps->length + 1);
				strcpy(ret, "./Maps/");
				strcat(ret, maps->name);
				maps = anfang;
				struct txt *tmp = maps;
				for (;id > 0; id--){
					tmp = maps->next;
					free(maps->name);
					free(maps);
					maps = tmp;
				}
				return ret;
			}
		}
	}else{
		return "./Maps/Map1.txt";
	}
}

int init(){
	system("wmctrl -r ':ACTIVE:' -N CletS-GO");//ändert Terminal Namen
	log = fopen("/tmp/spiel.log", "w");
	// Animation
	show_anim(15);
	read_config();
	// Startmenue
	zustand = startmenu();
	if (zustand == 3){
		show_endanim();
		if (fs){
			system("wmctrl -r ':ACTIVE:' -b toggle,fullscreen");//Macht Window zum Fullscreen oder beendet ihn
		}
		return 1;
	}
	// Map erstellen
	map_ptr = malloc(sizeof (struct map));
	read_map(map_ptr, getLink());
	return 0;

}

void read_var(FILE *file, int *adr){
	char c = fgetc(file);
	while (c != ' '){
		c = fgetc(file);
	}
	c = fgetc(file);
	if (c <= '9' && c >= '0'){
		*adr = 0;
	}
	while (c <= '9' && c >= '0'){
		*adr *= 10;
		*adr += c - '0';
		c = fgetc(file);
	}
}

void read_config(){
	FILE *file = fopen("./config.txt", "r");
	if (file == NULL){
		printf("---Doesn't found config.txt, using default values---\n");
		return;
	}
	read_var(file, &EXPLODETIME);
	read_var(file, &DMGTIME);
	read_var(file, &maxBombCount);
	read_var(file, &RADIUS);
}

//returns the char at (x, y)
char getc_arr(int x, int y){
	int zs = y * (map_ptr->breite) + x;
	char c = *(map_ptr->ptr + zs);
	// fprintf(log, "%d %d: %c (%d)\n", x, y, c, c);
	// fflush(log);

	return c;
}
int move(uint16_t button, player *p){
		// Invalide Taste?
		if(button > TB_KEY_ARROW_UP || TB_KEY_ARROW_RIGHT > button)
			return(1);

		// haben safe Pfeiltasten

		// kann man sich bewegen?
		int check_res = check(button, p);
		return(check_res);
}

int check(int direct, player *p){
	char c;
    switch (direct){
        case TB_KEY_ARROW_UP:
        	// border?
            c = getc_arr(p->x, (p->y) -1 );
            fprintf(log, "Zeichen oben %c (%d)\n", c, c);
            fflush(log);
    		if (c == '+' || c == '-' || c == '|' || c == '#')
    			return 1;
    		// alles gut
          	(p->y)--;
            break;
        case TB_KEY_ARROW_DOWN:
            // border?
            c = getc_arr(p->x, p->y +1 );
            fprintf(log, "Zeichen unten %c (%d)\n", c, c);
            fflush(log);
    		if (c == '+' || c == '-' || c == '|' || c == '#')
    			return 1;

    		// alles gut
            (p->y)++;
            break;
        case TB_KEY_ARROW_LEFT:
            // border?
            c = getc_arr(p->x -1 , p->y);
            fprintf(log, "Zeichen links %c (%d)\n", c, c);
            fflush(log);
    		if (c == '+' || c == '-' || c == '|' || c == '#')
    			return 1;

    		// alles gut
            (p->x)--;

            // noch ein Schritt?
			c = getc_arr(p->x -1 , p->y);
            fprintf(log, "Zeichen links %c (%d)\n", c, c);
            fflush(log);
    		if (c == '+' || c == '-' || c == '|' || c == '#')
    			return 0;

    		// alles gut
            (p->x)--;

            break;
        case TB_KEY_ARROW_RIGHT:
            // border?
            c = getc_arr(p->x +1, p->y);
            fprintf(log, "Zeichen rechts %c (%d)\n", c, c);
            fflush(log);
    		if (c == '+' || c == '-' || c == '|' || c == '#')
    			return 1;

    		// alles gut
            (p->x)++;

            // noch ein Schritt?
            c = getc_arr(p->x +1, p->y);
            fprintf(log, "Zeichen rechts %c (%d)\n", c, c);
            fflush(log);
    		if (c == '+' || c == '-' || c == '|' || c == '#')
    			return 0;

    		// alles gut
            (p->x)++;
            break;
        default:
        	return 1;
            break;
    }
    return 0;
}
int move2(uint32_t button, player *p2){
		// Invalide Taste gibt es nicht, wurde schon geprueft

		// haben safe w a s d

		// kann man sich bewegen?
		int check_res = check2(button, p2);
		return(check_res);
}

int check2(int direct, player *p2){
	char c;
    switch (direct){
        case 'w':
        	// border?
            c = getc_arr(p2->x, (p2->y) -1 );
            fprintf(log, "Zeichen oben %c (%d)\n", c, c);
            fflush(log);
    		if (c == '+' || c == '-' || c == '|' || c == '#')
    			return 1;
    		// alles gut
          	(p2->y)--;
            break;
        case 's':
            // border?
            c = getc_arr(p2->x, p2->y +1 );
            fprintf(log, "Zeichen unten %c (%d)\n", c, c);
            fflush(log);
    		if (c == '+' || c == '-' || c == '|' || c == '#')
    			return 1;

    		// alles gut
            (p2->y)++;
            break;
        case 'a':
            // border?
            c = getc_arr(p2->x -1 , p2->y);
            fprintf(log, "Zeichen links %c (%d)\n", c, c);
            fflush(log);
    		if (c == '+' || c == '-' || c == '|' || c == '#')
    			return 1;

    		// alles gut
            (p2->x)--;

            // noch ein Schritt?
            c = getc_arr(p2->x -1 , p2->y);
            fprintf(log, "Zeichen links %c (%d)\n", c, c);
            fflush(log);
    		if (c == '+' || c == '-' || c == '|' || c == '#')
    			return 0;

    		// alles gut
            (p2->x)--;

            break;
        case 'd':
            // border?
            c = getc_arr(p2->x +1, p2->y);
            fprintf(log, "Zeichen rechts %c (%d)\n", c, c);
            fflush(log);
    		if (c == '+' || c == '-' || c == '|' || c == '#')
    			return 1;

    		// alles gut
            (p2->x)++;

            // noch ein Schritt?
            c = getc_arr(p2->x +1, p2->y);
            fprintf(log, "Zeichen rechts %c (%d)\n", c, c);
            fflush(log);
    		if (c == '+' || c == '-' || c == '|' || c == '#')
    			return 0;

    		// alles gut
            (p2->x)++;
            break;
        default:
        	return 1;
            break;
    }
    return 0;
}

int startmenu(void){
	//Optionen im Startmenü:

	//Local Multiplayer
		//2 players
		//3 players
		//4 players
	//Online Multiplayer
		//Host Game
		//Join Game

	printf("Welcome!\n");
	printf("\t- press [1] to play local multiplayer\n");
	printf("\t- press [2] to learn how to play the game\n");
    printf("\t- press [3] to quit the game\n");
	printf("> ");
	
	char c;
	while(1){
		c = getchar();
		switch(c){
			case '1':
				printf("local multiplayer\n");
				while (c != '\n'){
					c = getchar();
				}
				return 1;
			case '2':
				printf("How to play:\n");
				printf("\t- player 1 moves with [ARROW_UP], [ARROW_LEFT], [ARROW_DOWN], [ARROW_RIGHT] and places bombs with [Enter]\n");
				printf("\t- player 2 moves with [W], [A], [S], [D] and places bombs with [Space]\n");
        		printf("Rules:\n");
//        		printf("\t- One game consists of several rounds\n");
        		printf("\t- A round will end when one player dies\n");
//        		printf("\t- The first player to win 3 rounds wins the game\n");
				printf("Tips:\n");
				printf("\t- Bombs can't destroy the solid blocks (-, +, |, #)\n");
				//printf("Bombs can destroy the destructible blocks (x)\n");
        		printf("\t- Do not walk into the explosions!\n");
				printf(" \n");
				printf("If you want to play now you can now press [1] or [2] to start the game or [3] to quit the game\n");
				break;
			case '3':
				return 3;
			case '\n':
				printf("> ");
				break;
			default:
				break;
			}
	}
	return(0);
}

int createTermbox(){
	int code = tb_init();
	if (code < 0) {
        fprintf(stderr, "termbox init failed, code: %d\n", code);
        return -1;
    }
    // Input- & OutputModus (viele Farben) der TB festlegen
    tb_select_input_mode(TB_INPUT_ESC | TB_INPUT_MOUSE);
    tb_select_output_mode(TB_OUTPUT_256);

    // cleart die TB einmal - macht sie "startklar"
    tb_clear();

	// Map wird gezeichnet
	char *ptr = map_ptr->ptr;
	for(int i = 0; i < map_ptr->hoehe; i++)			// y-Koordinate
		for(int j = 0; j < map_ptr->breite; j++){		// x-Koordinate
			tb_change_cell(j, i, *ptr, map_ptr->vg, map_ptr->hg);
			ptr++;						// position Pointer erhoehen

	}


    tb_present();
	// Eine Figur anlegen
	player *player1 = malloc(sizeof(player));
	player1->x = map_ptr->spawnAx;
	player1->y = map_ptr->spawnAy;
	player1->ch = '1';
	player1->bombs = 3;
	player1->isDead = 0;
	player1->isActive = 0;

	player *player2 = malloc(sizeof(player));
	player2->x = map_ptr->spawnBx;
	player2->y = map_ptr->spawnBy;
	player2->ch = '2';
	player2->bombs = 3;
	player2->isDead = 0;
	player2->isActive = 0;

	

	fprintf(log, "breite: %d \t hoehe: %d\n", map_ptr->breite, map_ptr->hoehe);
	fflush(log);	

	// BombenArray, DAMAGEArray
	bomb bomb_Array[maxBombCount];
	for (int i = 0; i < maxBombCount; i++) {
		bomb_Array[i].x = 0;
		bomb_Array[i].y = 0;
		bomb_Array[i].radius = 0;
		bomb_Array[i].timer = 0;
		bomb_Array[i].isActive = 0;
	}
	// DAMAGEArray
	// int dmg_Array[map_ptr->breite*map_ptr->hoehe];
	// for(int i = 0; i < map_ptr->breite*map_ptr->hoehe; i++)	
	// 	dmg_Array[i] = 0;

	int *dmg_Array = malloc(map_ptr->breite * map_ptr->hoehe * sizeof(int));
	for(int i = 0; i < map_ptr->breite * map_ptr->hoehe; i++){
		*dmg_Array = 0;
		dmg_Array += 1;
	}
	dmg_Array -= map_ptr->breite * map_ptr->hoehe;
		

	// Figur auf Termbox registrieren
	tb_change_cell(player1->x, player1->y, player1->ch, map_ptr->vg,map_ptr->hg);
	tb_change_cell(player2->x, player2->y, player2->ch, map_ptr->vg,map_ptr->hg);

	// Zeichnet neu
	tb_present();

	// AB HIER: EVENTS
	// AB HIER: EVENTS
	// AB HIER: EVENTS
	// AB HIER: EVENTS
	// AB HIER: EVENTS

	// Bombentimer
	clock_t start = clock();
	clock_t end;

	// BombenRefresh [Inventar]
	clock_t start_bomb = clock();
	clock_t end_bomb;

	while(winner == -1){	

		struct tb_event event;
		int tb_input = tb_peek_event(&event,5);		// welche Art von Input (Maus, Tastatur)

		// Falls Fehler
		if(tb_input == -1){
			fprintf(stderr, "Event-Error!");
			return(1);
		}
		// Was für ein Event?
		switch(tb_input){
			// Tastatureingabe
			case TB_EVENT_KEY :
				// ESC = Ende
				if(event.key == TB_KEY_ESC){
					tb_shutdown();
					return(0);
				}
				// PFEILTASTEN
				else if(TB_KEY_ARROW_RIGHT <= event.key && event.key <= TB_KEY_ARROW_UP){
					// eine andere Taste wurde gedrueckt
					int oldX1 = player1->x;
					int oldY1 = player1->y;
					// move() checks if pressed KEY is valid (= player moves) 
					// 			|
					//			+--> with parameter tb_input
					//
					// if not(move_res = 1), termbox will do nothing
					// if yes(move_res = 0), termbox will print changed array_slots (oldX, oldY, newX, newY)
					int move_res = move(event.key, player1);								// ACHTUNG: HIER NOCH struct-player-Array übergeben !!!!!!!!!!!!!!!!!!!!!!!!
					// Keine Bewegung, weil ungueltige Taste oder Wand
					if(move_res == 1){
						;
						// do nothting
					}
					else{
						int newX1 = player1->x;
						int newY1 = player1->y;
						// SPIELER 1 AKTUALISIEREN
						tb_change_cell(oldX1, oldY1, ' ', map_ptr->vg, map_ptr->hg);			// alte Position ist nun Space
						tb_change_cell(newX1, newY1, player1->ch, map_ptr->vg, map_ptr->hg);	// neue das Spieler-Zeichen
						// GEGENSPIELER AKTUALLISIEREN
						tb_change_cell(player2->x, player2->y, player2->ch, map_ptr->vg, map_ptr->hg);	// neue das Spieler-Zeichen

						tb_present();														// TB neu printen
					}
				}
				// Spieler2 bewegt sich
				else if(event.ch == 'w' || event.ch == 'a' || event.ch == 's' || event.ch == 'd'){
					int oldX2 = player2->x;
					int oldY2 = player2->y;
					int move_res = move2(event.ch, player2);								// ACHTUNG: HIER NOCH struct-player-Array übergeben !!!!!!!!!!!!!!!!!!!!!!!!
					// Keine Bewegung, weil ungueltige Taste oder Wand
					if(move_res == 1){
						;					 
						// do nothting
					}
					else{
						int newX2 = player2->x;
						int newY2 = player2->y;
						// SPIELER 2 AKTUALLISIEREN
						tb_change_cell(oldX2, oldY2, ' ', map_ptr->vg, map_ptr->hg);			// alte Position ist nun Space
						tb_change_cell(newX2, newY2, player2->ch, map_ptr->vg, map_ptr->hg);	// neue das Spieler-Zeichen
						// GEGENSPIELER AKTUALLISIERUNG
						tb_change_cell(player1->x, player1->y, player1->ch, map_ptr->vg, map_ptr->hg);	// neue das Spieler-Zeichen

						tb_present();														// TB neu printen
					}

				}
				// Player 1 Bombe
				else if(event.key == TB_KEY_ENTER){
					// Bombe legen
					plantBomb(player1, bomb_Array);
				}
				else if(event.key == TB_KEY_SPACE){
					plantBomb(player2, bomb_Array);
				}
				else{
					;
				}
				break;
			default :
				;
		} // switch
		end = clock();
		end_bomb = clock();

		// ANFANG BOMBENTICK
		if( ( (double) (end-start) / CLOCKS_PER_SEC )> 0.01){
			start = clock();

			// tickt runter
			tickBombs(dmg_Array, bomb_Array);


			// ALLE BOMBEN PRINTEN
			for(int i = 0; i < maxBombCount; i++){
				if(bomb_Array[i].isActive)
					// BOMBE ROT (VG)
					tb_change_cell(bomb_Array[i].x, bomb_Array[i].y,'o',TB_BLACK,map_ptr->hg);
			}
	

			// dmg-Map akuallisieren	- 	BOMBEN RADIUS wird gezeichnet
			for(int i = 0; i < map_ptr->hoehe; i++){
				// Animation der Bomben
				for(int j = 0; j < map_ptr->breite; j++){
					if(*dmg_Array != 0){
						// wenn Schaden von 1 --> 0, dann reprint()
						if(*dmg_Array == 1){
							// BOMBE RESET (HG)
							tb_change_cell(j,i,' ',map_ptr->vg, map_ptr->hg);
						}
						else{
							// EXPLOSION = ROT (HG)
							tb_change_cell(j,i,' ',map_ptr->vg,TB_BLACK);
						}
						(*dmg_Array)--;
					}
					dmg_Array++;
				}
			}
			dmg_Array -= map_ptr->breite * map_ptr->hoehe;
			tb_present();
		} // ENDE BOMBENTICK

		// BOMBEN REFRESH
		if( ( (double) (end_bomb - start_bomb) / CLOCKS_PER_SEC )> 0.03){
			start_bomb = clock();

			// BombenCap Player1 erreicht?
			if(player1->bombs < 3)
				(player1->bombs)++;

			// BombenCap Player2 erreicht?
			if(player2->bombs < 3)
				(player2->bombs)++;

		} // ENDE BOMBEN REFRESH


		// pruefe ob Spieler in Bombe gerannt ist lel (AUF DEM DMG ARRAY)
		int player1_i = player1->x + player1->y * map_ptr->breite;
		int player2_i = player2->x + player2->y * map_ptr->breite;

		// DMG-Array zeigt dahin wo spieler steht auf map

		// Player 1 tot?
		dmg_Array += player1_i;
		if(0 < *dmg_Array)
			player1->isDead = 1;
		dmg_Array -= player1_i;

		// Player2 tot?
		dmg_Array += player2_i;
		if(0 < *dmg_Array)
			player2->isDead = 1;
		dmg_Array -= player2_i;


		// Gewinner?
		if(player1->isDead){
			if(player2->isDead)
				winner = 0;		// unentschieden
			else
				winner = 2;		// 1 gewinnt
		}
		else if(player2->isDead)
			winner = 1;			// 2 gewinnt

		if(winner != -1){
			// tb_change_cell(player1->x, player1->y, player1->ch, map_ptr->vg, map_ptr->hg);
			// tb_change_cell(player2->x, player2->y, player2->ch, map_ptr->vg, map_ptr->hg);
			sleep(2);
		}
		
	} // while
	tb_shutdown();
	return(0);
}


// BOMBEN BOMBEN BOMBEN BOMBEN BOMBEN BOMBEN BOMBEN

// findet einen freien Bombenslot (im bombs-array) und gibt ihn aus
// das Bomben-array wird so aufgeteilt, dass der erste Spieler die ersten x
// Bombenslots zugewiesen werden, und auf die einzelnen ueber die bombsPlaced
// Variable zugegriffen wird
void plantBomb(player *p, bomb *bomb_Array) {
	// prueft #bomben eines Spieleers
	if (p->bombs) {
		int slot = findFreeBombSlot(bomb_Array);
		// belege ihn, wenn es einen freien Bombenslot gibt
		if (slot != -1) {
			bomb_Array += slot;
			bomb_Array->x = p->x;
			bomb_Array->y = p->y;
			bomb_Array->radius = RADIUS; 
			bomb_Array->timer = EXPLODETIME;
			bomb_Array->isActive = 1;
			(p->bombs)--;
			bomb_Array -= slot;

		}
	}
}
int findFreeBombSlot(bomb *bomb_Array) {
	for (int i = 0; i < maxBombCount; i++) {
		bomb_Array += i;
		if (! bomb_Array->isActive) {
			bomb_Array -= i;
			return (i);
		}else{
			bomb_Array -= i;
		}
	}
	return (-1); // falls kein Slot frei ist wird -1 zurueckgegeben
}


// Zaehlt den TImer fuer Bombe runter
void tickBombs(int *dmg_Array, bomb *bomb_Array) {
	for (int i = 0; i < maxBombCount; i++){
		// Index hin
		bomb_Array += i;
		// wenn der Bombenslot aktiv ist
		if (bomb_Array->isActive) {
			if (bomb_Array->timer < 1) {
				explosion(dmg_Array, bomb_Array);
				bomb_Array->isActive = 0;
			} else {
				(bomb_Array->timer)--;
			}
			
		}
		// Index back
		(bomb_Array) -= i;
	}
}
// void explodeBomb(bomb *bomb_Array) {
// 	explosion(bomb_Array->x, bomb_Array->y, EXPLODETIME, RADIUS); 
// }
void explosion(int *dmg_Array, bomb *bomb_Array){

	// Kopie vom Array
	int *copy = dmg_Array;

	// x-y Koordinate der Bombe zwischenspeichern
	int x = bomb_Array->x;
	int y = bomb_Array->y;

	// verschiebe Bomben-Array-Pointer zur Bombe (fuer Index)
	copy += x + y * (map_ptr->breite);

	*copy = DMGTIME;
	int up = 0;
	int down = 0;
	int left = 0;
	int right = 0;
	// expandiere Explosion solange der naechste Schritt kein solid ist
	// (bis radius erreich ist)
	while (directionNonSolid(x, y-up, 'u') && up < RADIUS){
		up++;
		// eins nach oben verschieben
		copy -= map_ptr->breite * up; 
		*copy = DMGTIME;
		copy += map_ptr->breite * up;
	}
	while (directionNonSolid(x, y+down, 'd') && down < RADIUS){
		down++;
		// eins nach unten verschieben
		copy += map_ptr->breite * down; 
		*copy = DMGTIME;
		copy -= map_ptr->breite * down;
	}
	while (directionNonSolid(x-left, y, 'l') && left < RADIUS){
		left++;
		// eins nach links verschieben
		copy -= left; 
		*copy = DMGTIME;
		copy += left;
	}
	while (directionNonSolid(x+right, y, 'r') && right < RADIUS){
		right++;
		// eins nach rechts verschieben
		copy += right; 
		*copy = DMGTIME;
		copy -= right;
	}
	copy -= x + y * (map_ptr->breite);
}

// ist dort ein solid?
int directionNonSolid(int x, int y, char direction){
	switch(direction){
		case 'u' :
			if (y == 0)
				return(1);
			else{
				char c = getc_arr(x,y-1);
				// returnt 1, wenn solid, und 0 wenn nicht
				return(!(c == '+' || c == '-' || c == '|' || c == '#'));
			}
		case 'd' :
			if (y == map_ptr->hoehe) 
				return(1);
			else {
				char c = getc_arr(x,y+1);
				// returnt 1, wenn solid, und 0 wenn nicht
				return(!(c == '+' || c == '-' || c == '|' || c == '#'));
			}
		case 'l' :
			if (x == 0) 
				return(1);
			else {
				char c = getc_arr(x-1, y);
				// returnt 1, wenn solid, und 0 wenn nicht
				return(!(c == '+' || c == '-' || c == '|' || c == '#'));	
			}
		case 'r' :
			//FIXME ARRAY_XMAX durch Array-Breite ersetzen
			if (x == map_ptr->breite)
				return(1);
			else {
				char c = getc_arr(x+1,y);
				// returnt 1, wenn solid, und 0 wenn nicht
				return(!(c == '+' || c == '-' || c == '|' || c == '#'));
			}
		default:
			return(1);
	}
}


uint16_t hex_to_int(char c){
	uint16_t ret = 0;
	if (c >= '0' && c <= '9'){
		ret = (uint16_t) c - '0';
	}else if(c >= 'a' && c <= 'f'){
		ret = c - 'a' + 10;
	}else if(c >= 'A' && c <= 'F'){
		ret = c - 'A' + 10;
	}
	return ret;
}

void read_map(struct map *ptr, char *str){
	if (strcmp(str, "-generate-") == 0){
		start_generating();
		return;
	}
	FILE *file = fopen(str, "r");
	char c = getc(file);
	//Höhe:
	ptr->hoehe = 0;
	while (c != ','){
		ptr->hoehe *= 10;
		ptr->hoehe += c-'0';
		c = getc(file);
	}
	c = getc(file);//für ' '

	//Breite:
	c = getc(file);//erste Zahl
	ptr->breite = 0;
	while (c != ','){
		ptr->breite *= 10;
		ptr->breite += c-'0';
		c = getc(file);
	}
	c = getc(file);//für ' '

	//Hintergrund:
	c = getc(file);//erste Zahl
	ptr->hg = 0;
	while (c != ','){
		ptr->hg *= 16;
		ptr->hg += hex_to_int(c);
		c = getc(file);
	}
	c = getc(file);//für ' '

	//Vordergrund
	c = getc(file);//erste Zahl
	ptr->vg = 0;
	while (c != '\n'){
		ptr->vg *= 16;
		ptr->vg += hex_to_int(c);
		c = getc(file);
	}

	//Spawns von A:
	c = getc(file);//erste Zahl
	ptr->spawnAx = 0;
	while (c != ','){
		ptr->spawnAx *= 10;
		ptr->spawnAx += c-'0';
		c = getc(file);
	}
	c = getc(file);//für ' '
	c = getc(file);//erste Zahl von y koord.
	ptr->spawnAy = 0;
	while (c != ','){
		ptr->spawnAy *= 10;
		ptr->spawnAy += c-'0';
		c = getc(file);
	}
	c = getc(file);//für ' '

	//Spawns von B:
	c = getc(file);//erste Zahl
	ptr->spawnBx = 0;
	while (c != ','){
		ptr->spawnBx *= 10;
		ptr->spawnBx += c-'0';
		c = getc(file);
	}
	c = getc(file);//für ' '
	c = getc(file);//erste Zahl von y koord.
	ptr->spawnBy = 0;
	while (c != '\n'){
		ptr->spawnBy *= 10;
		ptr->spawnBy += c-'0';
		c = getc(file);
	}

	//Char Array b hier:
	int elm = ptr->hoehe * ptr->breite;
	ptr->ptr = malloc(elm);
	for (int i = 0; i < ptr->hoehe; i++){
		c = getc(file);
		for (int j = 0; j < ptr->breite; j++){
			*(ptr->ptr) = c;
			(ptr->ptr)++;
			c = getc(file);
		}
		while (c != '\n'){
			c = getc(file);
		}
	}
	(ptr->ptr) -= elm;

	fclose(file);
}

//Animationen:

void show_anim(int time) {
	system("clear");
	char clets[9][68] = {
	"+-----------------------------------------------------------------+",
	"|   #######         ##                       ##        #######    |",
	"| ###    ###        ##          #####        ##       #########   |",
	"| ##                ##       ####    ###   ######      ###        |",
	"| ##          ####  ##      ##      ###      ##         ####      |",
	"| ##                ##      ########         ##           ####    |",
	"| ###    ###        ####    ##       ##      ##        ########   |",
	"|   #######         #####    #########       ###      ########    |",
	"|=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=|"};
	for (int i = 0; i < 9; i++){
		printf(BOLD BLUE "%s\n" RESET, clets[i]);
	}
	usleep(10000 * time);
	const char ptr[17][68] = { 
	 "|       #################                   ###########           |",
	 "|   #######################            #####################      |",
	 "|  #########################         #########################    |",
	 "| #########          ########       #######             #######   |", 
	 "| ######              #######      ######                 ######  |",
	 "| ######                           ######                 ######  |", 
	 "| ######                           ######                 ######  |", 
	 "| ######                           ######                 ######  |", 
	 "| ######                           ######                 ######  |", 
	 "| ######               #######     ######                 ######  |", 
	 "| ######               #######     ######                 ######  |", 
	 "| #######             ########     ######                 ######  |",
	 "| ########            ########      #######            ########   |",
	 "|  ####################### ###       #########################    |",
	 "|   ###################### ###         #####################      |",
	 "|      #################                    ###########           |",
	 "+-----------------------------------------------------------------+"};
	for (int i = 0; i < 17; i++) {
		printf(BOLD BLUE "%s\n" RESET, ptr[i]);
		usleep(1000 * time);
	}
	puts("");
}

void show_endanim(){
	system("clear");
	const char ptr[26][146+1] = { 
	"+-----------------------------------------------------------------+-------------------------------------------------------------------------------",
	"|   #######         ##                       ##        #######    |                                                                               ",
	"| ###    ###        ##          #####        ##       #########   |   ##################      #####                 #####   ####################  ",
	"| ##                ##       ####    ###   ######      ###        |  ######################    #####               #####    ####################  ",
	"| ##          ####  ##      ##      ###      ##         ####      |  #######################    #####             #####     ####################  ",
	"| ##                ##      ########         ##           ####    |  ######          #######     #####           #####      #####                 ",
	"| ###    ###        ####    ##       ##      ##        ########   |  #####            ######      #####         #####       #####                 ",
	"|   #######         #####    #########       ###      ########    |  #####            ######       #####       #####        #####                 ",
	"|=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=|  #####           ######         #####     #####         #####                 ",
	"|       #################                   ###########           |  #####         ######            #####   #####          #####                 ",
	"|   #######################            #####################      |  ######      #######              ##### #####           #####                 ",
	"|  #########################         #########################    |  ##################                #########            ####################  ",
	"| #########          ########       #######             #######   |  ####################               #######             ####################  ", 
	"| ######              #######      ######                 ######  |  ######################              #####              ####################  ",
	"| ######                           ######                 ######  |  ######         ########             #####              #####                 ", 
	"| ######                           ######                 ######  |  #####            #######            #####              #####                 ", 
	"| ######                           ######                 ######  |  #####             #######           #####              #####                 ", 
	"| ######                           ######                 ######  |  #####             #######           #####              #####                 ", 
	"| ######               #######     ######                 ######  |  #####             #######           #####              #####                 ", 
	"| ######               #######     ######                 ######  |  #####            #######            #####              #####                 ", 
	"| #######             ########     ######                 ######  |  ######         ########             #####              #####                 ",
	"| ########            ########      #######            ########   |  ######################              #####              ##################### ",
	"|  ####################### ###       #########################    |  ####################                #####              ##################### ",
	"|   ###################### ###         #####################      |   ################                   #####              ##################### ",
	"|      #################                    ###########           |                                                                               ", 
	"+-----------------------------------------------------------------+-------------------------------------------------------------------------------"};
	for (int i = 67; i < 146; i += 2){
		usleep(10000);
		system("clear");
		for (int j = 0; j < 26; j++){
			char *tmp = malloc(i+1);
			strncpy(tmp, ptr[j], i);
			tmp += i;
			*tmp = '\0';
			tmp -= i;
			printf(BOLD BLUE "%s" RESET, tmp);
			if (j == 0 || j == 25){
				printf(BOLD BLUE "-+\n" RESET);
			}else{
				printf(BOLD BLUE " |\n" RESET);
			}
			free(tmp);
		}
	}

} 
