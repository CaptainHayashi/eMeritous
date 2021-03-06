/**************************************************************************
 *  mapgen.h                                                              *
 *                                                                        *
 *  Copyright 2007, 2008 Lancer-X/ASCEAI                                  *
 *  Copyright 2010       CaptainHayashi etc.                              *
 *                                                                        *
 *  This file is part of Meritous.                                        *
 *                                                                        *
 *  Meritous is free software: you can redistribute it and/or modify      *
 *  it under the terms of the GNU General Public License as published by  *
 *  the Free Software Foundation, either version 3 of the License, or     *
 *  (at your option) any later version.                                   *
 *                                                                        *
 *  Meritous is distributed in the hope that it will be useful,           *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *  GNU General Public License for more details.                          *
 *                                                                        *
 *  You should have received a copy of the GNU General Public License     *
 *  along with Meritous.  If not, see <http://www.gnu.org/licenses/>.     *
 *                                                                        *
 **************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <assert.h>
#include <limits.h>

#include <SDL.h>

#include "dungeon.h"
#include "graphics.h"
#include "screens.h"
#include "boss.h"
#include "mapgen.h"
#include "save.h"
#include "levelblit.h"

void NewLevel();
	
void SaveLevel();

int Generate();

int DoRepeat = 0;

int place_of_power = 0;

GameLevel map;
int total_rooms = 0;

int rdir = 0;

int next_check = 30;

int GetRoom(int x, int y);

int rndnum(int max)
{
  return rand() % (max+1);
}

int r_fails[4] = {0};
int r_successes[4] = {0};

unsigned char floortiles[4] = {12, 18, 19, 20};

void
write_room_data (SaveFile *f, Room *rm)
{
  struct RoomConnection *rt;

  save_file_write_int (f, rm->x);
  save_file_write_int (f, rm->y);
  save_file_write_int (f, rm->w);
  save_file_write_int (f, rm->h);
  save_file_write_int (f, rm->creator);
  save_file_write_int (f, rm->visited);
  save_file_write_int (f, rm->checkpoint);
  save_file_write_int (f, rm->s_dist);
  save_file_write_int (f, rm->connections);
  save_file_write_int (f, rm->room_type);
  save_file_write_int (f, rm->room_param);

  for (rt = rm->con; rt != NULL; rt = rt->n)
    {
      save_file_write_int (f, rt->x);
      save_file_write_int (f, rt->y);
      save_file_write_int (f, rt->x2);
      save_file_write_int (f, rt->y2);
      save_file_write_int (f, rt->c);
    }
}

void
read_room_data (SaveFile *f, Room *rm)
{
  int i;
  struct RoomConnection *rt;
	
  rm->x = save_file_read_int (f);
  rm->y = save_file_read_int (f);
  rm->w = save_file_read_int (f);
  rm->h = save_file_read_int (f);
  rm->creator = save_file_read_int (f);
  rm->visited = save_file_read_int (f);
  rm->checkpoint = save_file_read_int (f);
  rm->s_dist = save_file_read_int (f);
  rm->connections = save_file_read_int (f);
  rm->room_type = save_file_read_int (f);
  rm->room_param = save_file_read_int (f);
	
  rm->con = NULL;
	
  rm->enemies = 0;
	
  for (i = 0; i < rm->connections; i++) {
    rt = rm->con;
    rm->con = malloc (sizeof (struct RoomConnection));
    rm->con->x = save_file_read_int (f);
    rm->con->y = save_file_read_int (f);
    rm->con->x2 = save_file_read_int (f);
    rm->con->y2 = save_file_read_int (f);
    rm->con->c = save_file_read_int (f);
    rm->con->n = rt;
  }
}

/* Write map data to a save file. */

void
write_map_data (SaveFile *f)
{
  int i;

  save_file_write_int (f, map.w);
  save_file_write_int (f, map.h);
  save_file_write_int (f, map.totalRooms);
  save_file_write_int (f, place_of_power);

  for (i = 0; i < map.w * map.h; i++)
    {
      save_file_write_char (f, map.m[i]);
      save_file_write_int (f, map.r[i]);

      if ((i % 7447) == 7446)
        SavingScreen (0, (float)i / (float) (map.w * map.h));
    }

  for (i = 0; i < map.totalRooms; i++)
    {
      write_room_data (f, &(map.rooms)[i]);

      if ((i % 85) == 84)
        SavingScreen (1, (float)i / (float) map.totalRooms);
    }
}

void
read_map_data (SaveFile *f)
{
  int i;

  map.w = save_file_read_int (f);
  map.h = save_file_read_int (f);
  map.totalRooms = total_rooms = save_file_read_int (f);
  place_of_power = save_file_read_int (f);

  for (i = 0; i < map.w * map.h; i++)
    {
      map.m[i] = save_file_read_char (f);
      map.r[i] = save_file_read_int (f);

      if ((i % 7447) == 7446)
        LoadingScreen(0, (float)i / (float)(map.w * map.h));
    }

  for (i = 0; i < map.totalRooms; i++)
    {
      read_room_data (f, &(map.rooms)[i]);

      if ((i % 85) == 84)
        LoadingScreen (1, (float)i / (float) map.totalRooms);
    }
}

int rndval(int a, int b)
{
  int temp;
	
  if (a == b) {
    return a;
  }
	
  if (b < a) {
    temp = a;
    a = b;
    b = temp;
  }
	
  temp = rndnum(b - a);
	
  return temp + a;
}

void RandomGenerateMap()
{
  int trying = 1;
  if (game_load) {
    NewLevel();
    /* FIXME: de-globalise this. */
    read_map_data (&save);
  } else {
    NewLevel();
    while (trying) {
		
      trying = !Generate();
    }
  }
  /* SaveLevel(); */
}

void NewLevel()
{
  int x, y;
  unsigned char *map_p;
	
  map.w = 512;
  map.h = 512;
	
  map.m = malloc(map.w * map.h * sizeof(unsigned char));
  map.r = malloc(map.w * map.h * sizeof(int));
  map_p = map.m;
	
  for (y = 0; y < map.h; y++) {
    for (x = 0; x < map.w; x++) {
      *(map_p++) = 17;
      map.r[y*map.w+x] = -1;
    }
  }
}

void DestroyDungeon()
{
  int i;
  struct RoomConnection *c, *d;
	
  /* Destroy map */
  free(map.m);
  free(map.r);
	
  /* Destroy rooms */
  for (i = 0; i < total_rooms; i++) {
    c = map.rooms[i].con;
    while (c != NULL) {
      d = c;
      c = c->n;
      free(d);
    }
  }
  total_rooms = 0;
}

void ResetLevel()
{
  int x, y;
  unsigned char *map_p;
	
  map.w = 512;
  map.h = 512;
  map_p = map.m;
	
  total_rooms = 0;

  rdir = 0;

  next_check = 30;
	
  for (y = 0; y < map.h; y++) {
    for (x = 0; x < map.w; x++) {
      *(map_p++) = 17;
      map.r[y*map.w+x] = -1;
    }
  }
}

void SaveLevel()
{
  int x, y, i;
  SDL_Surface *map_surf;
  char cs[2] = ".";
  char rnum[5] = "0000";
  unsigned char ch;
  unsigned char *map_p;
  SDL_Color cpalette[4];
  Uint8 cl;
	
  map_surf = SDL_CreateRGBSurface(0, 4096, 4096, 8, 0, 0, 0, 0);
	
  map_p = map.m;
	
  cpalette[0].r = cpalette[0].g = cpalette[0].b = 0;
  cpalette[1].r = cpalette[1].g = cpalette[1].b = 255;
  cpalette[2].r = 255; cpalette[2].g = 0; cpalette[2].b = 255;
  cpalette[3].r = 0; cpalette[3].g = 255; cpalette[3].b = 128;
	
  SDL_SetPalette(map_surf, SDL_LOGPAL | SDL_PHYSPAL, cpalette, 0, 4);
	
  for (y = 0; y < map.h; y++) {
    for (x = 0; x < map.w; x++) {
      ch = *(map_p++);
			
      if (IsSolid(ch))
        *cs = 4;
      else
        *cs = 5;
				
      if (ch == 17)
        *cs = 0;
				
      cl = 1;
      if (map.rooms[GetRoom(x, y)].room_type == 2) cl = 2;
      if (map.rooms[GetRoom(x, y)].room_type == 3) cl = 3;
			
      draw_map_text (x * 8, y * 8, cs, cl, map_surf);
    }
  }
  for (i = 0; i < NUM_ROOMS; i++) {
    sprintf(rnum, "%d", i);
    draw_map_text (map.rooms[i].x * 8, map.rooms[i].y * 8, rnum, 0, map_surf);
  }
	
  SDL_SaveBMP(map_surf, "map.bmp");
}

void CreateRoomDimensions(int *w, int *h)
{
  *w = rndval(5, 12);
  *h = rndval(5, 12);
	
  if (*w == 12) {
    *w = rndval(12, 15);
  }
  if (*h == 12) {
    *h = rndval(12, 15);
  }
}

void Put(int x, int y, unsigned char tile, int room)
{
  map.m[map.w * y + x] = tile;
  map.r[map.w * y + x] = room;
}

unsigned char Get(int x, int y)
{
  if (x < 0) return 17;
  if (y < 0) return 17;
  if (x >= map.w) return 17;
  if (y >= map.h) return 17;
	
  return map.m[map.w*y+x];
}

int
GetRoom (int x, int y)
{
  if (x < 0 
      || y < 0
      || x >= map.w
      || y >= map.h)
    return -1;

  return map.r[map.w * y + x];
}

int
GetVisited (int x, int y)
{
  /* Not a valid room. */
  if (x < 0 
      || y < 0
      || x >= map.w
      || y >= map.h
      || GetRoom (x, y) == -1)
    return 0;

  return map.rooms[GetRoom (x, y)].visited;
}

void Paint(int xp, int yp, int w, int h, char *fname)
{
  FILE *fp;
  int x, y;
  fp = fopen(fname, "rb");
	
  for (y = 0; y < h; y++) {
    for (x = 0; x < w; x++) {
      Put(x+xp, y+yp, fgetc(fp), GetRoom (x + xp, y + yp));
    }
  }
  fclose(fp);
}

void DrawRoom(int place_x, int place_y, int room_w, int room_h, int room_id)
{
  int x, y, i;
  int f_type;
	
  f_type = rand()%4;
  /* Corners */
  Put(place_x, place_y, 11, room_id);
  Put(place_x + room_w - 1, place_y, 10, room_id);
  Put(place_x, place_y + room_h - 1, 9, room_id);
  Put(place_x + room_w - 1, place_y + room_h - 1, 8, room_id);
	
  /* Walls */
	
  for (i = 0; i < room_w - 2; i++) {
    Put(place_x + 1 + i, place_y + room_h - 1, 4, room_id);
    if (rand() % 16 == 0) Put(place_x + 1 + i, place_y + room_h - 1, 45 + (rand()%2)*4, room_id);
    Put(place_x + 1 + i, place_y, 5, room_id);
    if (rand() % 16 == 0) Put(place_x + 1 + i, place_y, 46 + (rand()%2)*4, room_id);
  }

  for (i = 0; i < room_h - 2; i++) {
    Put(place_x + room_w - 1, place_y + 1 + i, 6, room_id);
    if (rand() % 16 == 0) Put(place_x + room_w - 1, place_y + 1 + i, 47 + (rand()%2)*4, room_id);
    Put(place_x, place_y + 1 + i, 7, room_id);
    if (rand() % 16 == 0) Put(place_x, place_y + 1 + i, 48 + (rand()%2)*4, room_id);
  }
	
  /* Floor */
	
  for (y = 0; y < room_h - 2; y++) {
    for (x = 0; x < room_w - 2; x++) {
      Put(place_x + 1 + x, place_y + 1 + y, floortiles[f_type], room_id);
    }
  }

  /* Magic Tiles */

  if ((room_id % 30) == 29) {
    if (Get(place_x + 1 + rand()%(room_w-2), place_y + 1 + rand()%(room_h-2)) == floortiles[f_type]) {
      Put(place_x + 1 + rand()%(room_w-2), place_y + 1 + rand()%(room_h-2), 28+rand()%3, room_id);
    }
  }

  /* Save tiles */
	
  if ((room_id % 25) == 20) {
    x = place_x + 1 + rand()%(room_w-2);
    y = place_y + 1 + rand()%(room_h-2);
    if (Get(x, y) == floortiles[f_type]) {
      Put(x, y, 31, room_id);
    }
  }

  /* Summon tiles */
  if ((room_id % 75) == 48) {
    x = place_x + 1 + rand()%(room_w-2);
    y = place_y + 1 + rand()%(room_h-2);
    if (Get(x, y) == floortiles[f_type]) {
      Put(x, y, 32, room_id);
    }
  }
	
  /* Compass tile */
	
  if ((room_id % 20) == 19) {
    x = place_x + 1 + rand()%(room_w-2);
    y = place_y + 1 + rand()%(room_h-2);
    if (Get(x, y) == floortiles[f_type]) {
      Put(x, y, 53, room_id);
    }
  }

  /* NOTE: Most of these equations here are based on conjecture. 
     They may work fine for the default number of rooms (3000) but 
     could break for different values. */

  /* First room */
  if (room_id == 0) {
    Paint(place_x+1, place_y+1, room_w-2, room_h-2, "dat/d/centre.loc");
  }

  /* PSI key rooms */
  if ((room_id % (NUM_ROOMS / NUM_REGULAR_BOSSES)) == 
      ((NUM_ROOMS / NUM_REGULAR_BOSSES / 2) - 1)) {
    Paint(place_x+1, place_y+1, room_w-2, room_h-2, "dat/d/weapon.loc");
  }

  /* Boss rooms */
  if ((room_id % (NUM_ROOMS / NUM_REGULAR_BOSSES)) == 
      ((NUM_ROOMS / NUM_REGULAR_BOSSES) - 1)) {
    Paint(place_x+1, place_y+1, room_w-2, room_h-2, "dat/d/bossroom.loc");
  }
}

int NoRoomCollision(int place_x, int place_y, int room_w, int room_h)
{
  int x, y;
	
  if (place_x < 0) return 0;
  if (place_y < 0) return 0;
  if ((place_x+room_w) > map.w) return 0;
  if ((place_y+room_h) > map.h) return 0;

  for (y = 0; y < room_h; y++) {
    for (x = 0; x < room_w; x++) {
      if (Get(place_x + x, place_y + y) != 17) return 0;
    }
  }
	
  return 1;
}

void MakeConnect(int x, int y, int type)
{
  int nx, ny;
  int d1, d2;
  int room_1, room_2;
  struct RoomConnection *rconnect;
	
  switch (type) {
  case 0:
    nx = x;
    ny = y - 1;
    d1 = 14;
    d2 = 13;
    break;
  case 1:
    nx = x;
    ny = y + 1;
    d1 = 13;
    d2 = 14;
    break;
  case 2:
    nx = x - 1;
    ny = y;
    d1 = 16;
    d2 = 15;
    break;
  case 3:
    nx = x + 1;
    ny = y;
    d1 = 15;
    d2 = 16;
    break;
  default:
    nx = 0;
    ny = 0;
    d1 = 0;
    d2 = 0;
    break;
  }
	
  room_1 = GetRoom(x, y);
  room_2 = GetRoom(nx, ny);
  if ((room_1 % (NUM_ROOMS / 3)) == ((NUM_ROOMS / 3) - 1)) {
    d1 = d1 - 13 + 21;
    d2 = d2 - 13 + 38;
  } else {
    if ((room_2 % (NUM_ROOMS / 3)) == ((NUM_ROOMS / 3) - 1)) {
      d1 = d1 - 13 + 38;
      d2 = d2 - 13 + 21;
    }
  }
  Put(x, y, d1, GetRoom(x, y));
  Put(nx, ny, d2, GetRoom(nx, ny));

  map.rooms[room_1].connections++;
  rconnect = map.rooms[room_1].con;
  map.rooms[room_1].con = malloc(sizeof(struct RoomConnection));
  map.rooms[room_1].con->n = rconnect;
  map.rooms[room_1].con->x = x;
  map.rooms[room_1].con->y = y;
  map.rooms[room_1].con->x2 = nx;
  map.rooms[room_1].con->y2 = ny;
  map.rooms[room_1].con->c = room_2;
	
  map.rooms[room_2].connections++;
  rconnect = map.rooms[room_2].con;
  map.rooms[room_2].con = malloc(sizeof(struct RoomConnection));
  map.rooms[room_2].con->n = rconnect;
  map.rooms[room_2].con->x = nx;
  map.rooms[room_2].con->y = ny;
  map.rooms[room_2].con->x2 = x;
  map.rooms[room_2].con->y2 = y;
  map.rooms[room_2].con->c = room_1;

}

int SuitableConnection(int t)
{
  switch (t) {
  case 4:
  case 5:
  case 6:
  case 7:
		
  case 45:
  case 46:
  case 47:
  case 48:
		
  case 49:
  case 50:
  case 51:
  case 52:
    return 1;
    break;
			
  default:
    break;
  }
  return 0;
}

void NewRoom(int place_x, int place_y, int room_w, int room_h, int creator)
{
  int connect_points = 0;
  int cplist_x[100], cplist_y[100], cplist_r[100], cplist_t[100];
	
  int sr_cps = 0;
  int sr_cp[100];
	
  int sr_nps = 0;
  int sr_np[100];
	
  int i;
	
  /* Draw this room */
  map.rooms[total_rooms].checkpoint = 0;
  DrawRoom(place_x, place_y, room_w, room_h, total_rooms);
	
  map.rooms[total_rooms].x = place_x;
  map.rooms[total_rooms].y = place_y;
	
  map.rooms[total_rooms].w = room_w;
  map.rooms[total_rooms].h = room_h;
	
  map.rooms[total_rooms].room_type = 0;
  map.rooms[total_rooms].room_param = 0;
	
  map.rooms[total_rooms].creator = creator;
	
  map.rooms[total_rooms].connections = 0;
  map.rooms[total_rooms].con = NULL;
  map.rooms[total_rooms].enemies = 0;
	
  map.rooms[total_rooms].visited = 0;

  map.rooms[total_rooms].s_dist = -1;
	
  if (total_rooms == 0) {
    map.rooms[total_rooms].checkpoint = 1;
  }
	
	
	
  total_rooms++;

  if (creator == -1) return;
	
  /* Find connection points */
	
  for (i = 0; i < room_w - 2; i++) {
    if (SuitableConnection(Get(place_x + 1 + i, place_y - 1))) {
      cplist_x[connect_points] = place_x + 1 + i;
      cplist_y[connect_points] = place_y;
      cplist_r[connect_points] = GetRoom(place_x + 1 + i, place_y - 1);
      cplist_t[connect_points] = 0;
      connect_points++;
    }
		
    if (SuitableConnection(Get(place_x + 1 + i, place_y + room_h))) {
      cplist_x[connect_points] = place_x + 1 + i;
      cplist_y[connect_points] = place_y + room_h - 1;
      cplist_r[connect_points] = GetRoom(place_x + 1 + i, place_y + room_h);
      cplist_t[connect_points] = 1;
      connect_points++;
    }
  }
  for (i = 0; i < room_h - 2; i++) {
    if (SuitableConnection(Get(place_x - 1, place_y + 1 + i))) {
      cplist_x[connect_points] = place_x;
      cplist_y[connect_points] = place_y + 1 + i;
      cplist_r[connect_points] = GetRoom(place_x - 1, place_y + 1 + i);
      cplist_t[connect_points] = 2;
      connect_points++;
    }
		
    if (SuitableConnection(Get(place_x + room_w, place_y + 1 + i))) {
      cplist_x[connect_points] = place_x + room_w - 1;
      cplist_y[connect_points] = place_y + 1 + i;
      cplist_r[connect_points] = GetRoom(place_x + room_w, place_y + 1 + i);
      cplist_t[connect_points] = 3;
      connect_points++;
    }
  }
	
  for (i = 0; i < connect_points; i++) {
    if (cplist_r[i] == creator) {
      sr_cp[sr_cps++] = i;
    } else {
      sr_np[sr_nps++] = i;
    }
  }
	
  /* printf("cps: %d      room: %d\n", sr_cps, total_rooms); */

  assert(sr_cps > 0);
	
  i = rndval(0, sr_cps-1);
  MakeConnect(cplist_x[sr_cp[i]], cplist_y[sr_cp[i]], cplist_t[sr_cp[i]]);
	
  /* one other connection (if we can) */
  if (sr_nps > 0) {
    i = rndval(0, sr_nps-1);
    MakeConnect(cplist_x[sr_np[i]], cplist_y[sr_np[i]], cplist_t[sr_np[i]]);
  }

}

int AddChild(int room_id)
{
  Room r = map.rooms[room_id];
  int place_x = r.x;
  int place_y = r.y;
  int room_w = r.w;
  int room_h = r.h;
  int new_w, new_h, new_x, new_y;
  int room_pos;
	
  int trying;
  int attempts;
	

	
  trying = 1;
  attempts = 0;
  while (trying) {
    attempts++;
		
    if (( (total_rooms+1) % 500)==0) {
      new_w = 20;
      new_h = 15;
    } else {
      CreateRoomDimensions(&new_w, &new_h);
    }
		
    room_pos = (rdir++)%4;
		
    if (room_pos < 2) {
      /* vertical placement */
      new_x = rndval(place_x - (new_w - 3), place_x + (room_w - 3));
      if (room_pos == 0) {
        new_y = place_y - new_h;
      } else {
        new_y = place_y + room_h;
      }
    } else {
      /* horiz placement */
      new_y = rndval(place_y - (new_h - 3), place_y + (room_h - 3));
      if (room_pos == 2) {
        new_x = place_x - new_w;
      } else {
        new_x = place_x + room_w;
      }
    }
		
    if (NoRoomCollision(new_x, new_y, new_w, new_h)) {
      /* printf("SUCCESS\n"); */
      r_successes[room_pos]++;
      NewRoom(new_x, new_y, new_w, new_h, room_id);
      return 1;
    } else {
      /* printf("FAIL %d\n", attempts); */
      r_fails[room_pos]++;
      if (attempts > 20) return 0;
    }
  }
  return 0;
}

void RecurseSetDist()
{
  struct RoomConnection *rc;
  int queue[10000];
  int q_top = 1;
  int q_bot = 0;
  int rooms_left = NUM_ROOMS;
  int c_room;
  queue[0] = 0;
	
  /* Update the loading screen every 100 rooms. */
  if (rooms_left % 100 == 0) {
    LoadingScreen(1, 1.0 - ((float)rooms_left / (float)NUM_ROOMS));
  }
	
  map.rooms[0].s_dist = 0;
	
  while ((rooms_left > 0)) {	
    c_room = queue[q_bot];
    q_bot++;
    rooms_left--;
		
    rc = map.rooms[c_room].con;
		
    while (rc != NULL) {
      /* assert(qp < NUM_ROOMS); */
      if (map.rooms[rc->c].s_dist == -1) {
        queue[q_top] = rc->c;
        q_top++;
        map.rooms[rc->c].s_dist = map.rooms[c_room].s_dist+1;
      }
      rc = rc->n;
    }
  }
}

int RoomSize(int c_room)
{
  return sqrt(map.rooms[c_room].w * map.rooms[c_room].w
              + map.rooms[c_room].h * map.rooms[c_room].h);
}

void MakeSpecialRooms()
{
  int i, j;
  int c_tier;
  int c_room;
  int biggest_room_sz = 0;
  int biggest_room_n = -1;
  int rtyp[8] = {0};
  int ctyp;
  int x, y;
	
  /* Special rooms are:
   *
   * - Boss rooms, every 1/(number of bosses)th of the total rooms.
   *
   * - PSI key rooms, every 1/2(number of bosses)th of the total
   *   rooms that isn't a boss room.
   *
   * (By default, eg with 3000 rooms and 3 bosses, this will generate a 
   *  boss room in rooms 1000, 2000 and 3000 and a PSI key room in rooms 
   *  500, 1500 and 2500. TODO: test for other numbers)
   *
   * - Artifact rooms (biggest non-boss room of a given distance tier)
   *		Tiers: 5-9  10-14  15-19  20-24  25-29  30-34  35-39  40-44
   */
	
  /* boss rooms */
  for (i = 0; i < NUM_REGULAR_BOSSES; i++) {
    c_room = (((i + 1) * (NUM_ROOMS / NUM_REGULAR_BOSSES)) - 1);
    map.rooms[c_room].room_type = ROOM_BOSS;
    map.rooms[c_room].room_param = i;
  }
  /* power object rooms */
  for (i = 0; i < NUM_REGULAR_BOSSES; i++) {
    c_room = (((i + 1) * (NUM_ROOMS / NUM_REGULAR_BOSSES)) - 
              (NUM_ROOMS / NUM_REGULAR_BOSSES / 2) - 1);
    map.rooms[c_room].room_type = ROOM_PSI_KEY;
    map.rooms[c_room].room_param = i;
  }
	
  /* artifact rooms */
  for (c_tier = 0; c_tier < 8; c_tier++) {
    biggest_room_sz = 0;
    for (c_room = 0; c_room < NUM_ROOMS; c_room++) {
      if (map.rooms[c_room].room_type == 0) {
        if (map.rooms[c_room].s_dist >= (c_tier*5+5)) {
          if (map.rooms[c_room].s_dist <= (c_tier*5+9)) {
            if (RoomSize(c_room) > biggest_room_sz) {
              biggest_room_sz = RoomSize(c_room);
              biggest_room_n = c_room;
            }
          }
        }
      }
    }
    map.rooms[biggest_room_n].room_type = ROOM_ART_CHALLENGE;
		
    /* pick a # */
    for (;;) {
      ctyp = rand()%8;
      if (rtyp[ctyp] == 0) {
        rtyp[ctyp] = 1;
        break;
      }
    }
		
    map.rooms[biggest_room_n].room_param = ctyp;
		
    /*printf("Artifact room for tier %d is room %d (size %d), with artifact %d\n", c_tier, biggest_room_n, biggest_room_sz, ctyp); */
  }
	
  /* place of power
   * The room with the highest s_dist that is not of any other type */
	
  for (i = 0; i < NUM_ROOMS; i++) {
    if (map.rooms[i].s_dist > map.rooms[place_of_power].s_dist) {
      if (map.rooms[i].room_type == 0) {
        place_of_power = i;
      }
    }
  }

  map.rooms[place_of_power].room_type = ROOM_PLACE_OF_POWER;
	
  /* Now place some checkpoints in the remaining rooms
   * Normally, we would have a checkpoint for every 30
   * rooms, BUT since we aren't using that method any
   * more, we will simply use an equivalent--namely, to
   * divide the map into an 8x8 grid and place one
   * checkpoint per square.
   */

  for (y = 0; y < 8; y++) {
    for (x = 0; x < 8; x++) {
      j = -1;
      for (i = 0; i < 20; i++) {
        j = GetRoom(rand() % 64 + x * 64, rand() % 64 + y * 64);
                
        if (j >= 0) {
          if (map.rooms[j].room_type == 0) {
            Put(map.rooms[j].x + map.rooms[j].w / 2, map.rooms[j].y + map.rooms[j].h / 2, 25, j);
            map.rooms[j].checkpoint = 1;
            break;
          }
        }
      }
    }
  }
	
  next_check--;
}

int Generate()
{
  int attempts = 0;
  int i;
  int correct_dist = 0;
  int maxdist = 0;
  rdir = rand()%4;
  NewRoom(map.w / 2 - 20 / 2, map.h / 2 - 15 / 2, 20, 15, -1);
	
  for (attempts = 0; attempts < 100000; attempts++) {
    assert(map.w == 512);
    AddChild(rndval(rndval(0, total_rooms-1), total_rooms-1));
    if (total_rooms % 100 == 99) {
      LoadingScreen(0, (float)total_rooms / (float) NUM_ROOMS);
    }
    if (total_rooms == NUM_ROOMS) break;
  }
	
  if ((total_rooms < NUM_ROOMS)||(DoRepeat == 1)) {
    DoRepeat = 0;
    ResetLevel();
    return 0;
  }
	
  RecurseSetDist();
	
  for (i = 0; i < NUM_ROOMS; i++) {
    if (map.rooms[i].s_dist > maxdist) {
      maxdist = map.rooms[i].s_dist;
    }
		
    if (map.rooms[i].s_dist >= 50) {
      correct_dist = 1;
    }
  }
	
  if (correct_dist == 0) {
    /*printf("Dist fail (only %d)\n", maxdist);*/
    DoRepeat = 0;
    ResetLevel();
    return 0;
  }
	
  /*printf("Rooms: %d\n", total_rooms);*/
	
  MakeSpecialRooms();
	
  map.totalRooms = total_rooms;
  return 1;
}

