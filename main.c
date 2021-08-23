#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <string.h>

enum arrows
{
	left_arrow = 0x400,
	right_arrow = 0x401,
	up_arrow = 0x402,
	down_arrow = 0x403,
	del_key = 0x404
};

struct editor
{
	int cursor_x;
	int cursor_y;
	int rows;
	int cols;
	struct termios orig_termios;
};

struct buffer
{
	char *b;
	size_t len;
};

struct editor e;

int get_win_size(int *rows, int *cols)
{
	struct winsize ws;

	if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) return -1;
	else
	{
		*cols = ws.ws_col;
		*rows = ws.ws_row;
		return 0;
	}
}

#define INIT_BUFFER {NULL, 0}

void append_buffer(struct buffer *buf, const char *v, size_t len)
{
	char *new = realloc(buf->b, buf->len + len);

	if(new == NULL) return;

	memcpy(&new[buf->len], v, len);
	buf->b = new;
	buf->len += len;
}

void destroy_buffer(struct buffer *buf)
{
	free(buf->b);
}

void die(const char *msg)
{
	perror(msg);
	exit(EXIT_FAILURE);
}

void draw_rows(struct buffer *buf)
{
	for(int i = 0; i < e.rows; i++)
	{
		if(i == e.rows / 3)
		{
			char w[80];
			int len = snprintf(w, sizeof(w), "Moca Editor");
			int pad = (e.rows - len) / 2;
			if(pad)
			{
				append_buffer(buf, " ", 1);
				pad--;

				while(pad--) append_buffer(buf, " ", 1);
			}
			append_buffer(buf, w, len);
			append_buffer(buf, "\r\n", 2);
		}
		else append_buffer(buf, "~\r\n", 3);
		
		append_buffer(buf, "\x1b[K", 3);
		if(i == e.rows - 1) append_buffer(buf, "\r\n", 2);
	}
}

void clear_screen()
{
	struct buffer buf = INIT_BUFFER;
	
	append_buffer(&buf, "\x1b[?25l", 4);
	append_buffer(&buf, "\x1b[H", 3);

	draw_rows(&buf);

	char bf[32];
	snprintf(bf, sizeof(bf), "\x1b[%d;%dH", e.cursor_y + 1, e.cursor_x + 1);
	append_buffer(&buf, bf, strlen(bf));

	//append_buffer(&buf, "\x1b[H", 3);
	append_buffer(&buf, "\x1b[?25H", 4);
	write(STDOUT_FILENO, buf.b, buf.len);
	destroy_buffer(&buf);
}

void disableRaw()
{
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &e.orig_termios);
}

void enableRawMode()
{
	tcgetattr(STDIN_FILENO, &e.orig_termios);
	atexit(disableRaw);
	
	struct termios raw = e.orig_termios;
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	raw.c_oflag &= ~(OPOST);
	raw.c_cflag |= (CS8);
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
	
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int get_key()
{
	int c_;
	char c;
	while((c_ = read(STDIN_FILENO, &c, 1) != 1))
	{
		if(c_ == -1 && errno != EAGAIN) die("erro reading");
	}

	if(c == '\x1b')
	{
		char seq[3];

		if(read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
		if(read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

		if(seq[0] == '[')
		{
			if(seq[1] >= '0' && seq[1] <= '9')
			{
				if(seq[2] == '~')
				{
					switch(seq[1])
					{
						case '3': return del_key;
					}
				}
			}
			switch(seq[1])
			{
				case 'A': return up_arrow;
				case 'B': return down_arrow;
				case 'C': return right_arrow;
				case 'D': return left_arrow;
			}
		}
		return '\x1b';
	}
	return c;
}

void move_cursor(const int c)
{
	switch(c)
	{
		case left_arrow:
		{
			if(!(e.cursor_x - 1 < 0)) e.cursor_x--;
			break;
		}
		case down_arrow:
		{
			if(!(e.cursor_y + 1 > e.rows)) e.cursor_y++;
			break;
		}
		case right_arrow:
		{
			if(!(e.cursor_x + 1 > e.cols)) e.cursor_x++;
			break;
		}
		case up_arrow:
		{
			if(!(e.cursor_y - 1 < 0)) e.cursor_y--;
			break;
		}
	}
}

void editor_listen()
{
	int c = get_key();

	switch(c)
	{
		case 'q':
		{
			write(STDOUT_FILENO, "\033[H\033[J", 6);
			write(STDOUT_FILENO, "\x1b[H", 3);
			exit(EXIT_SUCCESS);
			break;
		}
		case left_arrow:
		case down_arrow:
		case up_arrow:
		case right_arrow:
		{
			move_cursor(c);
			break;
		}
	}
}

int main(void)
{
	if(get_win_size(&e.rows, &e.cols) == -1) die("Failed to initialize window size");
	e.cursor_x = 0;
	e.cursor_y = 0;
	enableRawMode();
	
	clear_screen();

	while(1)
	{
		clear_screen();
		editor_listen();
	}
}
