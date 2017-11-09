/*
  This code is under GPL - December 2016 - UVSQ
 */
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

#define UP    0 //A
#define DOWN  1 //B
#define LEFT  2 //C
#define RIGHT 3 //D

#define MAX_COL 80
#define MAX_LIN 25
#define MAX_COM  4 //Maximum number of movement commands  

#define ROCK  '#'    //ROCK
#define STAR  '.'    //STAR
#define SHIP  '0'    //SHIP in rotation mode
#define BOMB  '*'
#define LASER ' '

#define SHIP_U  '^'  //SHIP moving Up
#define SHIP_D  'v'  //SHIP moving Down
#define SHIP_L  '<'  //SHIP moving Left
#define SHIP_R  '>'  //SHIP moving Right

#define SPACE ' '    //SPACE

//Rudimentary graphics for Linux!!
#define clear() printf("\033[2J\033[1;1H") //system("clear");

//Not the most optimal set up ! (optimization : one dimentional grid :])
typedef struct { int tx; int ty; 
  
  int p1x; int p1y; int v1;
  int p2x; int p2y; int v2;
  int p3x; int p3y; int v3;
  
  int lines; int cols;
  int moves; int lasers;
  int laser_dist;
  char **grid; } universe;

//Look up table handling movements Up, Down, Right, Left
const char directions_LUT[MAX_COM] = { UP, DOWN, RIGHT, LEFT };

//Non buffered character keyboard input
char getch()
{
  struct termios old_tio, new_tio;
  unsigned char c;

  /* Get the terminal settings for stdin (standard input ==> Keyboard) */
  tcgetattr(STDIN_FILENO,&old_tio);

  /* Keep the old settings to restore when done */
  new_tio=old_tio;

  /* Disable canonical mode (buffered i/o) and local echo */
  new_tio.c_lflag &=(~ICANON & ~ECHO);

  /* Set the new settings immediately */
  tcsetattr(STDIN_FILENO,TCSANOW,&new_tio);
  
  c = getchar();
  
  /* Restore the former settings */
  tcsetattr(STDIN_FILENO,TCSANOW,&old_tio);
  
  return c;
}

//Generates a random int between in [x, y]
int randxy(int x, int y)
{ return (x + rand()) % y; }

//
int is_alpha(char c)
{ return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

//
int is_upper(char c)
{ return (c >= 'A' && c <= 'Z'); }

//Stencil !
void gen_crater(universe *u, char c, int x, int y, int r)
{
  u->grid[y][x] = c;
  
  for (int i = 1; i <= r; i++)
    {
      u->grid[(y + i) % (MAX_LIN - 1)][x] = SPACE;
      if (x - i > 0) u->grid[y][x - i] = SPACE;  
      u->grid[y][(x + i) % (MAX_COL - 1)] = SPACE; 
      if (y - i > 0) u->grid[y - i][x] = SPACE;			     
    }
}

//Dropping bombs/mines at random spots
void drop_bombs(universe *u, int n)
{
  int x, y;
  
  for (int i = 0; i < n; i++)
    {
      x = randxy(0, MAX_COL - 1);
      y = randxy(0, MAX_LIN - 1);
	
      if (u->grid[y][x] != '@' && u->grid[y][x] != '.')
	u->grid[y][x] = BOMB;
    }
}

//Generates a grid and places key the elements randomly 
universe *gen_universe(int lines, int cols)
{
  int x, y, r;
  int impacts = 100;
  universe *u = malloc(sizeof(universe));

  u->laser_dist = 1;
  u->moves = u->lasers = 0;
  u->lines = lines; u->cols = cols;
  u->grid = malloc(sizeof(char *) * lines);
  
  for (int i = 0; i < lines; i++)
    {
      u->grid[i] = malloc(sizeof(char) * cols);
      memset(u->grid[i], ROCK, cols);
    }
  
  /* central crater */
  gen_crater(u, SPACE, MAX_COL >> 1      , MAX_LIN >> 1, randxy(10, 20));
  
  /* 4 craters */
  gen_crater(u, SPACE, MAX_COL >> 2      , MAX_LIN >> 2, randxy(3, 8));
  gen_crater(u, SPACE, 3 * (MAX_COL >> 2), MAX_LIN >> 2, randxy(3, 8));
  
  gen_crater(u, SPACE, MAX_COL >> 2      , 3 * (MAX_LIN >> 2), randxy(3, 8));
  gen_crater(u, SPACE, 3 * (MAX_COL >> 2), 3 * (MAX_LIN >> 2), randxy(3, 8));
  
  for (int i = 0; i < impacts;)
    {
      x = randxy(2, u->cols - 2);
      y = randxy(2, u->lines - 2);
      r = randxy(1, 4);
      
      if (u->grid[y][x] != SPACE)
	{
	  gen_crater(u, SPACE, x, y, r);
	  i++;
	}
    }
  
  //Lower crater !! 
  gen_crater(u, SPACE, MAX_COL - 3, MAX_LIN - 3, randxy(1, 4));
  
  u->tx = randxy(2, MAX_COL - 1); 
  u->ty = randxy(2, MAX_LIN);
  
  u->grid[u->ty][u->tx] = SHIP;
  
  u->p1x = randxy(2, MAX_COL - 1); 
  u->p1y = randxy(2, MAX_LIN);

  gen_crater(u, STAR, u->p1x, u->p1y, 3);

  u->p2x = randxy(2, MAX_COL - 1); 
  u->p2y = randxy(2, MAX_LIN);

  gen_crater(u, STAR, u->p2x, u->p2y, 3);

  u->p3x = randxy(2, MAX_COL - 1); 
  u->p3y = randxy(2, MAX_LIN);

  gen_crater(u, STAR, u->p3x, u->p3y, 3);
    
  return u;
}

//Yoda level 4-term comparison (could be optimized : should check the assembly code)
int check_supernova(universe *u, int x, int y)
{ return !((u->tx ^ x) | (u->ty ^ y)); }

//
int move_UP(universe *u)
{
  int safe = 1;
  
  if (u->ty)
    if (u->grid[u->ty - 1][u->tx] != ROCK)
      {	
	safe = !(u->grid[u->ty - 1][u->tx] == BOMB);

	//Setting up the graphics grid
	u->grid[u->ty][u->tx] = SPACE;
	u->ty--; //Computing coordinates
	u->grid[u->ty][u->tx] = SHIP_U;
	
	u->moves++;
      }

  return safe;
}

//
int move_DOWN(universe *u)
{
  int safe = 1;
  
  if (u->ty < MAX_LIN - 1)
    if (u->grid[u->ty + 1][u->tx] != ROCK)
      {	      
	safe = !(u->grid[u->ty + 1][u->tx] == BOMB);

	//Setting up the graphics grid
	u->grid[u->ty][u->tx] = SPACE;
	u->ty++; //Computing coordinates
	u->grid[u->ty][u->tx] = SHIP_D;
	
	u->moves++;
      }

  return safe;
}

//
int move_LEFT(universe *u)
{
  int safe = 1;
  
  if (u->tx)
    if (u->grid[u->ty][u->tx - 1] != ROCK)
      {	      
	safe = !(u->grid[u->ty][u->tx - 1] == BOMB);

	//Setting up the graphics grid
	u->grid[u->ty][u->tx] = SPACE;
	u->tx--; //Computing coordinates
	u->grid[u->ty][u->tx] = SHIP_L;
	
	u->moves++;
      }

  return safe;
}

//
int move_RIGHT(universe *u)
{
  int safe = 1;
  
  if (u->tx < MAX_COL)
    if (u->grid[u->ty][u->tx + 1] != ROCK)
      {	      
	safe = !(u->grid[u->ty][u->tx + 1] == BOMB);

	//Setting up the graphics grid
	u->grid[u->ty][u->tx] = SPACE;
	u->tx++; //Computing coordinates
	u->grid[u->ty][u->tx] = SHIP_R;
	
	u->moves++;
      }
  
  return safe;
}

//Wrapper for the previous functions
int move(universe *u, int d)
{  
  switch (d)
    {
    case UP    : return move_UP(u);
    case DOWN  : return move_DOWN(u);
    case LEFT  : return move_LEFT(u); 
    case RIGHT : return move_RIGHT(u);
    }
}

//
void laser(universe *u)
{ gen_crater(u, SHIP, u->tx, u->ty, u->laser_dist); u->lasers++; }

//Prints the universe
void print_universe_grid(universe *u)
{  
  for (int i = 0; i < u->lines; i++)
    {
      for (int j = 0; j < u->cols; j++)
	printf("%c", u->grid[i][j]);
 
      printf("\n");
    }
}

//
int main(int argc, char **argv)
{
  srand(time(NULL));
  
  char c;
  int safe = 1;
  int done = 0;
  int snovas = 0;
  unsigned long long moves = 0;
  universe *u = gen_universe(MAX_LIN, MAX_COL);

  drop_bombs(u, 20);
  
  //To avoid a do-while ...
  clear();
  printf("Commands: Up/Down/Left/Right & Space for lasering. [Supernovas: %d] [Moves: %llu]\n",
	 snovas, moves);
  
  print_universe_grid(u);
  
  while (!done && safe && (c = getch()) != 'q')
    {
      clear();
      printf("Commands: Up/Down/Left/Right & Space for lasering. [Supernovas: %d] [Moves: %llu]\n",
	     snovas, moves);
      
      //Esc + ^[ + A ==> UP (yes, UP is three characters !)
      if (c == '\033') getch(), c = getch();

      if (is_alpha(c))
	if (!is_upper(c))
	  c += 32;
      
      if (c != LASER)
	//x - 'A' --> Hash function for mapping directions in the LUT
	safe = move(u, directions_LUT[c - 'A']);
      else
	laser(u);
      
      if (!u->v1)
	u->v1 = check_supernova(u, u->p1x, u->p1y), u->laser_dist += u->v1, snovas += u->v1;
      
      if (!u->v2)
	u->v2 = check_supernova(u, u->p2x, u->p2y), u->laser_dist += u->v2, snovas += u->v2;
      
      if (!u->v3)
	u->v3 = check_supernova(u, u->p3x, u->p3y), u->laser_dist += u->v3, snovas += u->v3;
      
      done = (u->v1 && u->v2 && u->v3);
      
      print_universe_grid(u);
      moves++;
    }
  
  if (!safe)
    printf("You set off a bomb. Game Over !\n");
  
  if (done && safe) 
    printf("You win with %d move(s) & %d laser shot(s).\n", u->moves, u->lasers);
  
  free(u->grid);
  
  return 0;
}
