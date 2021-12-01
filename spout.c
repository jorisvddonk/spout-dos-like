/* See LICENSE.md file for copyright, license and warranty details */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include "vendor/dos.h"
#include "spout.h"
#include "sintable.h"
#include "font.h"

#define FRAMERATE 50
#define MAX_GRAIN 500

const int pitch = 320;

const unsigned char MATSUMI[] = {
		//	 80,  77,  66,  80, 180,   0,   0,   0,   1,   0, 128,   0,  10,   0, 223, 119,
		//	160,   0,   0,   0,
		0, 0, 0, 0, 0, 0, 0, 34, 56, 68, 10, 4,
		80, 129, 202, 0, 0, 0, 0, 0, 8, 0, 8, 34, 73, 255, 127, 223,
		241, 241, 95, 0, 0, 0, 0, 0, 8, 0, 0, 71, 72, 254, 10, 5,
		67, 17, 68, 0, 1, 2, 59, 187, 137, 75, 136, 66, 164, 16, 81, 31,
		84, 225, 155, 0, 2, 25, 10, 168, 138, 74, 72, 135, 33, 255, 49, 5,
		97, 177, 78, 0, 2, 33, 58, 171, 142, 74, 72, 134, 32, 16, 23, 215,
		86, 77, 117, 0, 2, 33, 34, 170, 9, 74, 73, 2, 73, 255, 49, 21,
		176, 33, 78, 0, 1, 26, 59, 187, 137, 50, 73, 2, 76, 40, 81, 28,
		0, 193, 181, 0, 0, 0, 0, 0, 0, 0, 0, 2, 245, 199, 23, 211,
		240, 33, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0};

typedef struct
{
	short x, y;
} SVECTOR;

typedef struct
{
	long x, y;
} VECTOR;

typedef struct tagGRAIN
{
	struct tagGRAIN *next;
	struct tagGRAIN *prev;

	SVECTOR s, v;
	short pos;
	unsigned char color;
} GRAIN;

GRAIN *grainUseLink, *grainFreeLink;

unsigned char vbuff[128 * 88];
unsigned char vbuff2[128 * 128];
unsigned char pixels[SDL_WIDTH * SDL_HEIGHT];

GRAIN grain[MAX_GRAIN];
GRAIN *v2g[128 * 128];

SVECTOR box;

VECTOR mPos, mSpeed;
int mR;

int nGrain;

int ztime = FRAMERATE * 60, score = 0, height = 0, dispscore = 0;
int hiScore[2] = {0, 0};
int dispPos, upperLine, rollCount;
char score_path[512];

unsigned char *vBuffer = NULL;

int exec = 1;
int interval = 0;
int font_posX = 0, font_posY = 0, font_width = 4, font_height = 6;
unsigned char font_fgcolor = 3, font_bgcolor = 0, font_bgclear = 0;
const unsigned char *font_adr = FONT6;

void spout(int t, int x, int y);
void sweep(unsigned char c1, unsigned char c2);
void initGrain(void);
GRAIN *allocGrain(void);
GRAIN *freeGrain(GRAIN *current);
int pceFontPrintf(const char *fmt, ...);
void pceFontSetTxColor(int color);
void pceFontSetBkColor(int color);
void pceFontSetPos(int x, int y);
void pceFontSetType(int type);
void pceLCDTrans();
int pcePadGet();

//#include "config.h"

void pceAppInit(void)
{
	vBuffer = vbuff;
	interval = 1000 / FRAMERATE;

	memset(vbuff, 0, 128 * 88);

	snprintf(score_path, 512, "%s/%s", getenv("HOME"), ".spout.sco");

	{
		int fa;
		if ((fa = open(score_path, O_RDONLY)) != -1)
		{
			read(fa, (void *)hiScore, 8);
			close(fa);
		}
	}

	srand(1 /*SDL_GetTicks()*/);
}

void pceAppProc(int cnt)
{
	static int gamePhase = 0, gameover;

	int pad = pcePadGet();

	if ((pad & (PAD_C | TRG_D)) == (PAD_C | TRG_D) ||
			(pad & (TRG_C | PAD_D)) == (TRG_C | PAD_D))
	{
		if (gamePhase >= 2)
		{
			gamePhase = 0;
		}
		else
		{
			exec = 0;
		}
		pad = 0;
	}

	if (gamePhase == 4)
	{
		if (pad & (TRG_C))
		{
			gamePhase = 3;
		}
		return;
	}

	if (!(gamePhase & 1))
	{
		if (gamePhase == 0)
		{
			if (score > hiScore[0] || (score == hiScore[0] && height > hiScore[1]))
			{
				int fa;
				hiScore[0] = score;
				hiScore[1] = height;
				if ((fa = open(score_path, O_CREAT | O_WRONLY | O_TRUNC)) != -1)
				{
					write(fa, (void *)hiScore, 8);
					close(fa);
				}
			}
		}
		else
		{
			score = 0;
			dispscore = 0;
			height = -58;
			ztime = 60 * FRAMERATE;
		}

		{
			int i;
			for (i = 0; i < 128 * 128; i++)
			{
				v2g[i] = NULL;
			}
			initGrain();
			nGrain = 0;
		}

		if (gamePhase & 2)
		{
			memset(vbuff2, 0xd2, 128 * 128);
			memset(vbuff2 + 128 * 0, 0, 128 * 78);
			memset(vbuff2 + 128 * (128 - 32), 0, 128 * 32);
		}
		else
		{
			memset(vbuff2, 0, 128 * 128);
		}

		memset(vbuff, 0, 128 * 88);

		{
			int i;
			unsigned char *pC;

			pC = vbuff2;
			for (i = 0; i < 128; i++)
			{
				*pC++ = 0x0b;
				*pC++ = 0x0b;
				*pC++ = 0x0b;
				*pC++ = 0x0b;
				pC += 128 - 8;
				*pC++ = 0x0b;
				*pC++ = 0x0b;
				*pC++ = 0x0b;
				*pC++ = 0x0b;
			}
		}

		mPos.x = 40 * 256;
		mPos.y = 0 * 256;
		mSpeed.x = 0;
		mSpeed.y = 0;
		mR = 256 + (gamePhase & 2) * 224;

		dispPos = 0;
		upperLine = 0;
		gameover = 0;
		rollCount = 0;
		gamePhase++;

		memset(vbuff + 128, 0x03, 128);
		pceFontSetType(2 + 128);
		pceFontSetPos(0, 82);
		if (height > 0)
		{
			pceFontPrintf("time:%2d height:%4d score:%6d", (ztime + FRAMERATE - 1) / FRAMERATE, height % 10000, score % 1000000);
		}
		else
		{
			pceFontPrintf("time:%2d height:   0 score:%6d", (ztime + FRAMERATE - 1) / FRAMERATE, score % 1000000);
		}
		pceFontSetType(0);
	}

	if ((pad & TRG_C) && gamePhase == 3 && gameover == 0)
	{
		pceFontSetType(2 + 128);
		pceFontSetPos(64 - 7 * 4 / 2, 33);
		pceFontPrintf(" pause ");
		pceFontSetType(0);

		gamePhase = 4;

		pceLCDTrans();
		return;
	}

	if (gamePhase & 2)
	{
		if (gameover == 0)
		{
			if ((pad & PAD_RI))
			{
				mR = (mR - 16) & 1023;
			}
			else if ((pad & PAD_LF))
			{
				mR = (mR + 16) & 1023;
			}
			if ((pad & (PAD_A | PAD_B)))
			{
				mSpeed.x -= sintable[(256 + mR) & 1023] / 128;
				mSpeed.y += sintable[mR] / 128;
			}
			mSpeed.y += 8;

			if (mSpeed.x < -256 * 4)
			{
				mSpeed.x = -256 * 4;
			}
			else if (mSpeed.x > 256 * 4)
			{
				mSpeed.x = 256 * 4;
			}
			if (mSpeed.y < -256 * 4)
			{
				mSpeed.y = -256 * 4;
			}
			else if (mSpeed.y > 256 * 4)
			{
				mSpeed.y = 256 * 4;
			}

			mPos.x += mSpeed.x / 16;
			mPos.y += mSpeed.y / 16;

			if (mPos.x >= 125 * 256)
			{
				mPos.x = 124 * 256;
				gameover = 1;
			}
			else if (mPos.x <= 2 * 256)
			{
				mPos.x = 3 * 256;
				gameover = 1;
			}
			if (mPos.y >= 78 * 256)
			{
				mPos.y = 77 * 256;
				gameover = 1;
			}

			if (mPos.y < 40 * 256)
			{
				unsigned char *pC;
				int i, j, w, x1, x2;
				mPos.y += 256;
				upperLine = (upperLine - 1) & 127;
				height++;

				if (height > 0)
				{
					score++;
					if ((height & 127) == 0)
					{
						score += (ztime + FRAMERATE - 1) / FRAMERATE * 10;
						ztime += 60 * FRAMERATE;
						if (ztime > 99 * FRAMERATE)
						{
							ztime = 99 * FRAMERATE;
						}
						pceFontSetType(2 + 128);
						pceFontSetPos(4 * 5, 82);
						pceFontPrintf("%2d", (ztime + FRAMERATE - 1) / FRAMERATE);
						pceFontSetType(0);
					}
					pceFontSetType(2 + 128);
					pceFontSetPos(4 * 15, 82);
					pceFontPrintf("%4d", height % 10000);
					pceFontSetType(0);
				}

				if (upperLine == 111 && height > 0)
				{
					unsigned long *pL;
					pL = (unsigned long *)(vbuff2 + 128 * 108 + 4);
					while (pL < (unsigned long *)(vbuff2 + 128 * 109 - 4))
					{
						*pL++ = 0;
					}
					pL += 2;
					while (pL < (unsigned long *)(vbuff2 + 128 * 110 - 4))
					{
						*pL++ = 0xd3d3d3d3;
					}
					pL += 2;
					while (pL < (unsigned long *)(vbuff2 + 128 * 111 - 4))
					{
						*pL++ = 0;
					}
				}

				box.x = 20 - (height + 40) / 64;
				if (box.x < 4)
				{
					box.x = 4;
				}
				box.y = 20 - (height + 40) / 64;
				if (box.y < 4)
				{
					box.y = 4;
				}

				for (j = 0; j < 1; j++)
				{
					int x, y;
					x = 4 + (rand() % box.x);
					y = 4 + (rand() % box.y);
					pC = vbuff2 + ((upperLine - 20 - (rand() & 7)) & 127) * 128;
					x1 = 4 + (rand() % (120 - x));
					x2 = x;
					i = y;
					while (i > 0)
					{
						if (pC < vbuff2)
						{
							pC += 128 * 128;
						}
						pC += x1;
						w = x2;
						while (w > 0)
						{
							*pC++ = 0;
							w--;
						}
						pC -= x1 + x2 + 128;
						i--;
					}
				}

				sweep(0x13, 0xd2);
			}
		}
	}
	else
	{
		mPos.x = 7 * 256;
		mPos.y = 60 * 256;
		mR = 0;

		if ((rollCount & 7) == 0)
		{
			int i, j;

			if ((upperLine & 31) == 0)
			{
				unsigned long *pL;
				vBuffer = vbuff2 + ((upperLine - 24) & 127) * 128;
				pceFontSetBkColor(0);

				switch (upperLine / 32)
				{
				case 0:
					pL = (unsigned long *)(vbuff2 + 12 + ((upperLine - 24) & 127) * 128);
					for (i = 0; i < 16; i++)
					{
						for (j = 0; j < 26 / 2; j++)
						{
							*pL = 0x91919191;
							pL += 2;
						}
						if ((i & 7) == 3)
						{
							pL += 7;
						}
						else if ((i & 7) == 7)
						{
							pL += 5;
						}
						else
						{
							pL += 6;
						}
					}

					pceFontSetTxColor(0x03);
					pceFontSetType(1 + 128);
					pceFontSetPos(64 - 4 * 5, 0);
					pceFontPrintf("spout");
					break;

				case 2:
					pceFontSetTxColor(0xc3);
					pceFontSetType(2 + 128);
					pceFontSetPos(118 - 20 * 4, 0);
					pceFontPrintf("    height: %8d", hiScore[1] % 1000000);
					pceFontSetPos(118 - 20 * 4, 6);
					pceFontPrintf("high-score: %8d", hiScore[0] % 1000000);
					break;

				case 1:
				{
					const unsigned char *pS = MATSUMI;
					unsigned char *pD = vbuff2 + ((upperLine - 16) & 127) * 128;
					for (i = 0; i < 128 / 8 * 10; i++)
					{
						unsigned char t = *pS++;
						for (j = 0; j < 8; j++)
						{
							if (t & 0x80)
							{
								*pD = 0xc3;
							}
							pD++;
							t <<= 1;
						}
					}
				}
				break;
				}

				pceFontSetType(0);
				pceFontSetTxColor(0x03);
				pceFontSetBkColor(0);

				vBuffer = vbuff;
			}
			upperLine = (upperLine - 1) & 127;

			sweep(0x13, 0x00);
		}
	}

	rollCount++;

	{
		static int gx[] = {-2, 2, -1, 1, 0};
		int r, t;

		r = rand() & 3;
		t = gx[r];
		gx[r] = gx[r + 1];
		gx[r + 1] = t;
		if (gamePhase & 2)
		{
			if (gameover == 0 && (pad & (PAD_A | PAD_B)))
			{
				int i, t, x, y;
				for (i = 0; i < 5; i++)
				{
					t = mPos.x / 256 + gx[i] + ((mPos.y / 256 - 1 + abs(gx[i]) + dispPos) & 127) * 128;
					x = mSpeed.x / 16 + sintable[(256 + mR) & 1023] / 8;
					y = mSpeed.y / 16 - sintable[mR] / 8;
					spout(t, x, y);
				}
			}
		}
		else
		{
			int i, t;
			for (i = -1; i <= 2; i++)
			{
				t = 7 + i + ((60 - 1 + dispPos) & 127) * 128;
				spout(t, 512, -384);
			}
		}
	}

	{
		GRAIN *pG, *pG2;
		SVECTOR svt;

		pG = grainUseLink;
		while (pG)
		{
			int f = 0;
			unsigned char *c;

			pG->v.y += 8;

			pG->s.x += pG->v.x;
			pG->s.y += pG->v.y;

			*(vbuff2 + pG->pos) = 0;
			*(v2g + pG->pos) = NULL;

			if (pG->s.y >= 256)
			{
				do
				{
					pG->s.y -= 256;
					pG->pos = (pG->pos + 128) & 16383;
					c = (vbuff2 + pG->pos);

					if (*c)
					{
						if (*c & 0x04)
						{
							int r;
							pG2 = *(v2g + pG->pos);
							r = 31 - (rand() & 63);
							svt = pG->v;
							pG->v = pG2->v;
							pG2->v = svt;
							pG->v.x += r;
							pG2->v.x -= r;
						}
						else
						{
							pG->v.y = -pG->v.y / 2;
							pG->v.x += 15 - (rand() & 31);
							if (*c & 0xc0)
							{
								*c -= 0x40;
								if (!(*c & 0xc0))
								{
									*c = 0;
								}
							}
							if (pG->color & 0xc0)
							{
								pG->color -= 0x40;
							}
							else
							{
								pG->color = 0;
								f = 1;
							}
						}
						pG->pos = (pG->pos - 128) & 16383;
						break;
					}
				} while (pG->s.y >= 256);
			}
			else
			{
				while (pG->s.y <= -256)
				{
					pG->s.y += 256;
					pG->pos = (pG->pos - 128) & 16383;
					c = (vbuff2 + pG->pos);

					if (*c)
					{
						if (*c & 4)
						{
							pG2 = *(v2g + pG->pos);
							svt = pG->v;
							pG->v = pG2->v;
							pG2->v = svt;
						}
						else
						{
							pG->v.y = -pG->v.y / 2;
							if (*c & 0xc0)
							{
								*c -= 0x40;
								if (!(*c & 0xc0))
								{
									*c = 0;
								}
							}
							if (pG->color & 0xc0)
							{
								pG->color -= 0x40;
							}
							else
							{
								pG->color = 0;
								f = 1;
							}
						}
						pG->pos = (pG->pos + 128) & 16383;
						break;
					}
				}
			}

			if (pG->s.x >= 256)
			{
				do
				{
					pG->s.x -= 256;
					pG->pos = (pG->pos + 1) & 16383;
					c = (vbuff2 + pG->pos);

					if (*c)
					{
						if (*c & 4)
						{
							pG2 = *(v2g + pG->pos);
							svt = pG->v;
							pG->v = pG2->v;
							pG2->v = svt;
						}
						else
						{
							pG->v.x = -pG->v.x / 2;
							if (*c & 0xc0)
							{
								*c -= 0x40;
								if (!(*c & 0xc0))
								{
									*c = 0;
								}
							}
							if (pG->color & 0xc0)
							{
								pG->color -= 0x40;
							}
							else
							{
								pG->color = 0;
								f = 1;
							}
						}
						pG->pos = (pG->pos - 1) & 16383;
						break;
					}
				} while (pG->s.x >= 256);
			}
			else
			{
				while (pG->s.x <= -256)
				{
					pG->s.x += 256;
					pG->pos = (pG->pos - 1) & 16383;
					c = (vbuff2 + pG->pos);

					if (*c)
					{
						if (*c & 4)
						{
							pG2 = *(v2g + pG->pos);
							svt = pG->v;
							pG->v = pG2->v;
							pG2->v = svt;
						}
						else
						{
							pG->v.x = -pG->v.x / 2;
							if (*c & 0xc0)
							{
								*c -= 0x40;
								if (!(*c & 0xc0))
								{
									*c = 0;
								}
							}
							if (pG->color & 0xc0)
							{
								pG->color -= 0x40;
							}
							else
							{
								pG->color = 0;
								f = 1;
							}
						}
						pG->pos = (pG->pos + 1) & 16383;
						break;
					}
				}
			}

			if (f)
			{
				*(vbuff2 + pG->pos) = pG->color;
				nGrain--;
				*(v2g + pG->pos) = NULL;
				pG = freeGrain(pG);
			}
			else
			{
				*(vbuff2 + pG->pos) = pG->color;
				*(v2g + pG->pos) = pG;
				pG = pG->next;
			}
		}
	}

	dispPos = upperLine;

	{
		unsigned long *pL, *pL2, *pLe;
		pL = (unsigned long *)(vbuff + 2 * 128);
		pL2 = (unsigned long *)(vbuff2 + dispPos * 128);

		pLe = pL2 + 128 * 78 / 4;
		if (pLe > (unsigned long *)(vbuff2 + 128 * 128))
		{
			pLe = (unsigned long *)(vbuff2 + 128 * 128);
		}

		while (pL2 < pLe)
		{
			*pL = *pL2 & 0x03030303;
			pL++;
			pL2++;
		}

		pL2 = (unsigned long *)(vbuff2);
		while (pL < (unsigned long *)(vbuff + 128 * (78 + 2)))
		{
			*pL = *pL2 & 0x03030303;
			pL++;
			pL2++;
		}
	}

	{
		unsigned char *pC;
		pC = vbuff2 + mPos.x / 256 + ((mPos.y / 256 + dispPos) & 127) * 128;
		if (*pC != 0 && (*pC & 4) == 0)
		{
			gameover = *pC;
		}
	}

	{
		static int gPhase = 0;
		unsigned char *pC;
		int i, x, y;
		if (gameover == 0 && (gamePhase & 2))
		{
			x = mPos.x + sintable[(256 + mR) & 1023] * gPhase / 64;
			y = mPos.y - sintable[mR] * gPhase / 64;
			for (i = 0; i < 3; i++)
			{
				if (y >= 78 * 256)
				{
					break;
				}
				*(vbuff + x / 256 + (y / 256 + 2) * 128) = 3;
				x += sintable[(256 + mR) & 1023] / 16;
				y -= sintable[mR] / 16;

				if (y >= 78 * 256)
				{
					break;
				}
				*(vbuff + x / 256 + (y / 256 + 2) * 128) = 3;
				x += sintable[(256 + mR) & 1023] / 16;
				y -= sintable[mR] / 16;

				if (y >= 78 * 256)
				{
					break;
				}
				*(vbuff + x / 256 + (y / 256 + 2) * 128) = 3;
				x += sintable[(256 + mR) & 1023] * 2 / 16;
				y -= sintable[mR] * 2 / 16;
			}
			gPhase = (gPhase + 1) & 15;
		}

		pC = vbuff + mPos.x / 256 + (mPos.y / 256 + 2) * 128;
		*(pC - 129) = 0x03;
		*(pC - 128) = 0x03;
		*(pC - 127) = 0x03;
		*(pC - 1) = 0x03;
		*pC = 0x00;
		*(pC + 1) = 0x03;
		*(pC + 127) = 0x03;
		*(pC + 128) = 0x03;
		*(pC + 129) = 0x03;
	}

	if (gamePhase == 1)
	{
		if (pad & (TRG_A | TRG_B))
		{
			gamePhase = 2;
		}
	}
	else if (gameover)
	{
		if (pad & (TRG_A | TRG_B))
		{
			gamePhase = 0;
		}
	}

	if ((gamePhase & 2) && ztime && gameover == 0)
	{
		ztime--;
		if ((ztime % FRAMERATE) == 0)
		{
			pceFontSetType(2 + 128);
			pceFontSetPos(4 * 5, 82);
			pceFontPrintf("%2d", (ztime + FRAMERATE - 1) / FRAMERATE);
			pceFontSetType(0);
		}
		if (ztime == 0)
		{
			gameover = 1;
		}
	}

	if (dispscore < score)
	{
		dispscore++;
		if (dispscore < score)
		{
			dispscore++;
		}
		pceFontSetType(2 + 128);
		pceFontSetPos(4 * 26, 82);
		pceFontPrintf("%6d", dispscore % 1000000);
		pceFontSetType(0);
	}

	if (gamePhase == 3 && gameover != 0)
	{
		pceFontSetType(2 + 128);
		pceFontSetPos(64 - 11 * 4 / 2, 33);
		pceFontPrintf(" game over ");
		pceFontSetType(0);
	}

	pceLCDTrans();
}

void spout(int t, int x, int y)
{
	if (*(vbuff2 + t) == 0)
	{
		if (nGrain < MAX_GRAIN)
		{
			GRAIN *pG = allocGrain();

			pG->v.x = x;
			pG->v.y = y;
			pG->s.x = 0;
			pG->s.y = 0;

			pG->color = (2 + (rand() & 1)) + 4 + 64 * 3;

			pG->pos = t;
			*(vbuff2 + t) = pG->color;
			v2g[t] = pG;
			nGrain++;
		}
	}
}

void sweep(unsigned char c1, unsigned char c2)
{
	int i;

	unsigned char *pC = vbuff2 + 4 + 128 * ((upperLine + 77) & 127);
	for (i = 0; i < 120; i++)
	{
		if (*pC & 4)
		{
			GRAIN **ppG;
			ppG = v2g + (int)(pC - vbuff2);
			freeGrain(*ppG);
			*ppG = NULL;
			nGrain--;
		}
		*pC++ = c1;
	}

	pC += 8;
	if (pC >= vbuff2 + 128 * 128)
	{
		pC -= 128 * 128;
	}

	for (i = 0; i < 120; i++)
	{
		*pC++ = c2;
	}
}

void initGrain(void)
{
	int i;

	for (i = 0; i < MAX_GRAIN - 1; i++)
	{
		grain[i].next = &grain[i + 1];
	}
	grain[i].next = NULL;

	grainFreeLink = grain;
	grainUseLink = NULL;

	return;
}

GRAIN *allocGrain(void)
{
	GRAIN *current = grainFreeLink;

	if (current)
	{
		grainFreeLink = current->next;

		current->next = grainUseLink;
		current->prev = NULL;
		if (current->next)
		{
			current->next->prev = current;
		}
		grainUseLink = current;
	}

	return current;
}

GRAIN *freeGrain(GRAIN *current)
{
	GRAIN *next = current->next;

	if (next)
	{
		next->prev = current->prev;
	}
	if (current->prev)
	{
		current->prev->next = next;
	}
	else
	{
		grainUseLink = next;
	}

	current->next = grainFreeLink;
	grainFreeLink = current;

	return next;
}

void pceLCDTrans()
{
	int x, y;
	unsigned char *vbi, *bi;

	bi = pixels;
	for (y = 0; y < SDL_HEIGHT; y++)
	{
		vbi = vBuffer + (y / zoom) * 128;
		for (x = 0; x < SDL_WIDTH; x++)
		{
			*bi++ = *(vbi + x / zoom);
		}
		bi += pitch - SDL_WIDTH;
	}
}

int pcePadGet()
{
	static int pad = 0;
	int i = 0, op = pad & 0x00ff;

	int k[] = {
			KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
			KEY_NUMPAD8, KEY_NUMPAD2, KEY_NUMPAD4, KEY_NUMPAD6,
			KEY_X, KEY_Z, KEY_SPACE, KEY_RETURN,
			KEY_ESCAPE, KEY_LSHIFT, KEY_RSHIFT};

	int p[] = {
			PAD_UP, PAD_DN, PAD_LF, PAD_RI,
			PAD_UP, PAD_DN, PAD_LF, PAD_RI,
			PAD_A, PAD_B, PAD_A, PAD_B,
			PAD_C, PAD_D, PAD_D,
			-1};

	pad = 0;

	do
	{
		if (keystate(k[i]))
		{
			pad |= p[i];
		}
		i++;
	} while (p[i] >= 0);

	pad |= (pad & (~op)) << 8;

	return pad;
}

void pceFontSetType(int type)
{
	const int width[] = {5, 8, 4};
	const int height[] = {10, 16, 6};
	const unsigned char *adr[] = {FONT6, FONT16, FONT6};

	type &= 3;
	font_width = width[type];
	font_height = height[type];
	font_adr = adr[type];
}

void pceFontSetTxColor(int color)
{
	font_fgcolor = (unsigned char)color;
}

void pceFontSetBkColor(int color)
{
	if (color >= 0)
	{
		font_bgcolor = (unsigned char)color;
		font_bgclear = 0;
	}
	else
	{
		font_bgclear = 1;
	}
}

void pceFontSetPos(int x, int y)
{
	font_posX = x;
	font_posY = y;
}

int pceFontPrintf(const char *fmt, ...)
{
	unsigned char *adr = vBuffer + font_posX + font_posY * 128;
	char *pC;
	char c[1024];
	va_list argp;

	va_start(argp, fmt);
	vsprintf(c, fmt, argp);
	va_end(argp);

	pC = c;
	while (*pC)
	{
		int i, x, y;
		const unsigned char *sAdr;
		if (*pC >= 0x20 && *pC < 0x80)
		{
			i = *pC - 0x20;
		}
		else
		{
			i = 0;
		}
		sAdr = font_adr + (i & 15) + (i >> 4) * 16 * 16;
		for (y = 0; y < font_height; y++)
		{
			unsigned char c = *sAdr;
			for (x = 0; x < font_width; x++)
			{
				if (c & 0x80)
				{
					*adr = font_fgcolor;
				}
				else if (font_bgclear == 0)
				{
					*adr = font_bgcolor;
				}
				adr++;
				c <<= 1;
			}
			adr += 128 - font_width;
			sAdr += 16;
		}
		adr -= 128 * font_height - font_width;
		pC++;
	}
	return 0;
}

int main(int argc, char *argv[])
{
	setvideomode(videomode_320x240);
	setpal(0, 255, 255, 255);
	setpal(1, 170, 170, 170);
	setpal(2, 85, 85, 85);
	setpal(3, 0, 0, 0);
	int cnt = 0;
	int i;

	pceAppInit();

	while (exec)
	{
		waitvbl();

		pceAppProc(cnt);
		blit((SDL_WIDTH - (128 * zoom)) / 2, (SDL_HEIGHT - (88 * zoom)) / 2, pixels, SDL_WIDTH, SDL_HEIGHT, 0, 0, 128 * zoom, 88 * zoom);

		cnt++;

		if (keystate(KEY_ESCAPE))
		{
			exec = 0;
		}
	}

	return 0;
}
