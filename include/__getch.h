#ifndef __GETCH_H
#define __GETCH_H

#ifndef _LIB_H
# error "Never use __getch.h directly, use lib.h instead of it"
#endif

/* The key value */
#define INLD_KEY	-1	// Invalid
#define KEY_ESC		27	// Esc
#define KEY_UP		560	// Up
#define KEY_DOWN	561	// Down
#define KEY_LEFT	562	// Left
#define KEY_RIGHT	563	// Right
#define KEY_END		564	// End
#define KEY_HOME	565	// Home
#define KEY_BKSPE	127	// Backspace
#define KEY_TAB		9	// Tab
#define KEY_ENTER	10	// Enter

#define KEY_F(x)	(530+x)	// Fx

#define KEY_ALT(ch)   (ch + 1000)	// Alt + (ch)

/* The main function: getch */
int getch();

#endif /* __GETCH_H */
