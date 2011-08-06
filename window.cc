#include <ncurses.h>
#include <cstring>


WINDOW *create_newwin(int height, int width, int ystart, int xstart, int border);
void destroy_win(WINDOW *local_win);

int main(int argc, char *argv[])
{ WINDOW *output_win, *input_win;
  int output_xstart, output_ystart, output_width, output_height;
  int input_xstart, input_ystart, input_width, input_height;

  int ch;

  // Start curses mode
  initscr();
  // Don't echo() while we do getch
  noecho();

  // Line buffering disabled
  //cbreak();

  // I need F1
  keypad(stdscr, TRUE);   

  // never forget
  refresh();

  // Define output window
  output_xstart = 0;
  output_ystart = 0;
  output_height = LINES - 3;
  output_width = COLS;
  output_win = create_newwin(output_height, output_width, output_ystart, output_xstart, 0);
  // BORDER
  //output_win = create_newwin(output_height, output_width, output_ystart, output_xstart, 1);

  scrollok(output_win, true);
  idlok(output_win, true);
  wrefresh(output_win);

  // Define input window
  input_xstart = 0;
  input_ystart = LINES - 3;
  input_height = 3;
  input_width = COLS;
  input_win = create_newwin(input_height, input_width, input_ystart, input_xstart, 1);


  // Define state
  int state_xpos = COLS - 23;
  int state_ypos = LINES - input_height;

  char state[22];
  strncpy(state, "STATE_BROWSELIST_RCVD", 22);

  mvprintw(state_ypos, state_xpos, "%s", state);

  // Define cursor
  int output_xpos = output_xstart+1;
  int output_ypos = output_ystart+1;

  int input_xpos = input_xstart+1;
  int input_ypos = input_ystart+1;
  move(input_ypos, input_xpos);


  // Define input
  int index = 0;
  char input[80];
  memset(input, 0, 80);

  while((ch = getch()) != KEY_F(1))
  { switch(ch)
    { 
      case KEY_BACKSPACE:
        if (input_xpos > input_xstart+1) {
          index--;
          input[index] = (char)NULL;
          input_xpos--;
          mvaddch(input_ypos, input_xpos,' ');
          move(input_ypos, input_xpos);
        }
        break;
      case KEY_UP:
          wscrl(output_win, -1);
          output_ypos++;
          move(input_ypos, input_xpos);
          wrefresh(output_win);
        break;
      case KEY_DOWN:
          wscrl(output_win, 1);
          output_ypos--;
          move(input_ypos, input_xpos);
          wrefresh(output_win);
        break;
      case KEY_ENTER:
      case '\n':
        mvwprintw(output_win,output_ypos, output_xpos, "%s", input);
        wrefresh(output_win);
        output_ypos++;

        if(output_ypos == output_height) {
          wscrl(output_win, 1);
          output_ypos--;
          move(input_ypos, input_xpos);
          wrefresh(output_win);
        }


        memset(input, 0, 80);

        while (index > 0) {
          index--;
          input_xpos--;
          mvaddch(input_ypos, input_xpos,' ');
          move(input_ypos, input_xpos);
        }
        break;
      default:
        mvprintw(input_ypos, input_xpos, "%c", ch);
        input[index++] = ch;
        input_xpos++;
        break;
    }
  }
    
  endwin();     /* End curses mode      */
  return 0;
}

WINDOW *create_newwin(int height, int width, int ystart, int xstart, int border)
{ WINDOW *local_win;

  local_win = newwin(height, width, ystart, xstart);
  if (border == 1) {
    box(local_win, 0 , 0);    /* 0, 0 gives default characters 
           * for the vertical and horizontal
           * lines      */
  }
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
