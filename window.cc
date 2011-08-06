#include <ncurses.h>


WINDOW *create_newwin(int height, int width, int starty, int startx);
void destroy_win(WINDOW *local_win);

int main(int argc, char *argv[])
{ WINDOW *my_win;
  int startx, starty, width, height;
  int ch;

  initscr();      /* Start curses mode    */
  noecho();     /* Don't echo() while we do getch */

  // ENTER SPIELEREIEN
  //cbreak();     /* Line buffering disabled
  //raw();
  //nonl();

  keypad(stdscr, TRUE);   /* I need that nifty F1   */

  height = 3;
  width = COLS -1;
  starty = LINES - height;
  startx = 1;
  printw("Press F1 to exit");
  refresh();
  my_win = create_newwin(height, width, starty, startx);

  int xpos = 1;
  int ypos = 1;

  int input_xpos = startx+2;
  int input_ypos = LINES-height+1;
  mvprintw(LINES-height+1, xpos, "");
  char myinput[80];

  int index = 0;
  myinput[index++] = '>';
  

  while((ch = getch()) != KEY_F(1))
  { switch(ch)
    { //case KEY_LEFT:
      //  destroy_win(my_win);
      //  my_win = create_newwin(height, width, starty,--startx);
      //  break;
      //case KEY_RIGHT:
      //  destroy_win(my_win);
      //  my_win = create_newwin(height, width, starty,++startx);
      //  break;
      //case KEY_UP:
      //  destroy_win(my_win);
      //  my_win = create_newwin(height, width, --starty,startx);
      //  break;
      //case KEY_DOWN:
      //  destroy_win(my_win);
      //  my_win = create_newwin(height, width, ++starty,startx);
      //  break;  
      case KEY_DOWN:
      case KEY_ENTER:
      case '\n':
        mvprintw(ypos, xpos, "%s", myinput);
        ypos++;


        input_xpos = startx+2;
        for(int i = 0; i <= index; i++) {
          myinput[i] = (char)NULL;
          mvprintw(input_ypos, input_xpos+i, "%c", ' ');
        }
        index = 0;
        myinput[index++] = '>';
        mvprintw(input_ypos, input_xpos, "");
        break;
      default:
        mvprintw(input_ypos, input_xpos, "%c", ch);
        myinput[index++] = ch;
        input_xpos++;
        break;
    }
  }
    
  endwin();     /* End curses mode      */
  return 0;
}

WINDOW *create_newwin(int height, int width, int starty, int startx)
{ WINDOW *local_win;

  local_win = newwin(height, width, starty, startx);
  box(local_win, 0 , 0);    /* 0, 0 gives default characters 
           * for the vertical and horizontal
           * lines      */
  wrefresh(local_win);    /* Show that box    */

  return local_win;
}

void destroy_win(WINDOW *local_win)
{ 
  /* box(local_win, ' ', ' '); : This won't produce the desired
   * result of erasing the window. It will leave it's four corners 
   * and so an ugly remnant of window. 
   */
  wborder(local_win, ' ', ' ', ' ',' ',' ',' ',' ',' ');
  /* The parameters taken are 
   * 1. win: the window on which to operate
   * 2. ls: character to be used for the left side of the window 
   * 3. rs: character to be used for the right side of the window 
   * 4. ts: character to be used for the top side of the window 
   * 5. bs: character to be used for the bottom side of the window 
   * 6. tl: character to be used for the top left corner of the window 
   * 7. tr: character to be used for the top right corner of the window 
   * 8. bl: character to be used for the bottom left corner of the window 
   * 9. br: character to be used for the bottom right corner of the window
   */
  wrefresh(local_win);
  delwin(local_win);
}
