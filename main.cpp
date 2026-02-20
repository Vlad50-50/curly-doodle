#include <SDL2/SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <iostream>
#include <cmath>
#include <ctime>
#include <vector>
#include <string>
#include <sqlite3.h>

#define GAME_MAP_SIZE 512
#define TEXTURE_ATLAS_PATH "assets/texture-atlas.png"
#define TILE_SIZE 50
#define TILE_SIZE_ON_ATLAS 50
#define CAMERA_SPEED 10
#define PLAYER_RANGE 60 
#define GAME_FONT_PATH "assets/fonts/Roboto/static/Roboto-Light.ttf"
#define DATA_BASE "save"

using namespace std;

enum Type {
	COAL,
	COPPER,
	IRON,
	GOLD,
	STONE,
	
	PLAYER,
	SELECTED,
	VOID
};

struct Sprite {
	Type type;
	SDL_Rect position;
	bool isShown;
	unsigned short int hp;
};

struct Tile {
	Sprite sprite;
	short unsigned int value;
};

struct SaveTile {
	uint16_t type;
	uint16_t x;
	uint16_t y;
	uint16_t isShown;
	uint16_t hp;
	uint16_t value;
};

struct Item {
	Type type;
	short int count;
};

struct Player {
	Sprite sprite;	
	short int *items;
};

const short unsigned int
	SCREEN_SIZE[2] = {800, 600};

SDL_Window *win = NULL;
SDL_Renderer *ren = NULL;

SDL_Texture *texture_atlas = NULL;

TTF_Font *gameFont = NULL;

Tile game_map[GAME_MAP_SIZE][GAME_MAP_SIZE];

Tile templates[6] = {
	{{STONE, {0, 0, 0, 0}, false, 150}, 0},
	{{COAL, {0, 0, 0, 0}, false, 200}, 0},
	{{COPPER, {0, 0, 0, 0}, false, 200}, 0},
	{{IRON, {0, 0, 0, 0}, false, 250}, 0},
	{{GOLD, {0, 0, 0, 0}, false, 300}, 0},
	{{VOID, {0, 0, 0, 0}, false, 0}, 0}
};

Tile getRandomTile() {
	Tile temp_sprite;
	int index = rand() % 256;

	if (index >= 250) {
		return templates[3];
	}
	else if (index >= 200) {
		return templates[2];
	}
	else if (index >= 100) {
		return templates[0];
	}
	else if (index >= 80) {
		return templates[1];
	}
	else {
		return templates[4];
	}
}

void renderTile(Tile (&tiles)[GAME_MAP_SIZE][GAME_MAP_SIZE], SDL_Rect &camera) {
	for (int i = 0; i < GAME_MAP_SIZE; ++i) {
		for (int j = 0; j < GAME_MAP_SIZE; ++j) {
			if (!(tiles[i][j].sprite.isShown)) continue;			

			SDL_Rect temp_dstRect = {
				tiles[i][j].sprite.position.x - camera.x,
				tiles[i][j].sprite.position.y - camera.y,
				TILE_SIZE,
				TILE_SIZE
			};

			if (tiles[i][j].sprite.type == VOID) {
				SDL_SetRenderDrawColor(ren, 128, 128, 128, 255);
				SDL_RenderFillRect(ren, &temp_dstRect);
				game_map[i-1][j].sprite.isShown = true;
				game_map[i+1][j].sprite.isShown = true;
				game_map[i][j-1].sprite.isShown = true;
				game_map[i][j+1].sprite.isShown = true;
			}
	
			if (
				temp_dstRect.x + TILE_SIZE < 0 || temp_dstRect.x > camera.w ||
		    	temp_dstRect.y + TILE_SIZE < 0 || temp_dstRect.y > camera.h
			) continue;

			SDL_Rect temp_sourceRect = {
				TILE_SIZE_ON_ATLAS * tiles[i][j].sprite.type, 
				0,
				TILE_SIZE_ON_ATLAS,
				TILE_SIZE_ON_ATLAS
			};
			SDL_RenderCopy(ren, texture_atlas, &temp_sourceRect, &temp_dstRect);
		}
	}
}

Tile* getTileByScreenPos(int x, int y, SDL_Rect &camera) {
	int 
		worldX = x + camera.x,
		worldY = y + camera.y,
	
		tileX = worldX / 50,
		tileY = worldY / 50;

	if (
		tileX < 0 || tileX >= GAME_MAP_SIZE || 
		tileY < 0 || tileY >= GAME_MAP_SIZE
	) return nullptr;

	return &game_map[tileX][tileY];
}

bool isCursorInRange(Player &player, int mouseX, int mouseY) {
		int
			dx = (player.sprite.position.x + player.sprite.position.w / 2) - mouseX,
			dy = (player.sprite.position.y + player.sprite.position.h / 2) - mouseY,
			d = sqrt(dx*dx + dy*dy);
		
		return d < PLAYER_RANGE;	
}

void renderSellected(Player &player, SDL_Rect &camera) {	
	int mouseX, mouseY;
	SDL_GetMouseState(&mouseX, &mouseY);

	if (!isCursorInRange(player, mouseX, mouseY)) return;

	Tile* tile = getTileByScreenPos(mouseX, mouseY, camera);
	if (!tile || tile->sprite.type == VOID || !(tile->sprite.isShown)) return;

    int worldX = mouseX + camera.x;
    int worldY = mouseY + camera.y;

    int tileX = (worldX / TILE_SIZE) * TILE_SIZE;
    int tileY = (worldY / TILE_SIZE) * TILE_SIZE;

    SDL_Rect dst = {
        tileX - camera.x,
        tileY - camera.y,
        TILE_SIZE,
        TILE_SIZE
    };

    SDL_Rect src = {
        TILE_SIZE_ON_ATLAS * SELECTED,
        0,
        TILE_SIZE_ON_ATLAS,
        TILE_SIZE_ON_ATLAS
    };

    SDL_RenderCopy(ren, texture_atlas, &src, &dst);
}

void changeToVoid(SDL_Rect &camera, Player &player) {
		int mouseX, mouseY;
		SDL_GetMouseState(&mouseX, &mouseY);
	
		if (!isCursorInRange(player, mouseX, mouseY)) return;

		Tile* t = getTileByScreenPos(mouseX, mouseY, camera);
		
		if (
			!(t) || 
			t->sprite.type == VOID ||
			!(t->sprite.isShown)
		) return;
	
		player.items[t->sprite.type]++;
		
		t->sprite.type = VOID;
}

void renderPlayer(Player &player) {
	SDL_Rect dst = {
		player.sprite.position.x,
		player.sprite.position.y,
		player.sprite.position.w,
		player.sprite.position.h
	};
	SDL_Rect src = {
		TILE_SIZE_ON_ATLAS * player.sprite.type,	
		0,
		TILE_SIZE_ON_ATLAS,
		TILE_SIZE_ON_ATLAS
	};
	SDL_RenderCopy(ren, texture_atlas, &src, &dst);
}

void cameraMovement(Player &player, SDL_Rect &camera, short int x, short int y) {
	int tileX = (player.sprite.position.x + x);
	int tileY = (player.sprite.position.y + y);

	if (x > 0) {
		tileX = (player.sprite.position.x + x + player.sprite.position.w);
	}
	if (y > 0) {
		tileY = (player.sprite.position.y + y + player.sprite.position.h);
	}
 
	Tile *t = getTileByScreenPos(tileX, tileY, camera);

    if (!t) return;
    if (t->sprite.type != VOID)
        return;

    camera.x += x;
    camera.y += y;
}

void renderText(const char* msg, SDL_Rect dst, SDL_Color color) {

	SDL_Surface *temp_surf = TTF_RenderUTF8_Blended(
													gameFont, 
													msg,
													color
	);
	SDL_Texture *temp_texture = SDL_CreateTextureFromSurface(ren, temp_surf);
	
	SDL_QueryTexture(temp_texture, NULL, NULL, &dst.w, &dst.h);
	SDL_RenderCopy(ren, temp_texture, NULL, &dst);
	
	SDL_FreeSurface(temp_surf);	
	SDL_DestroyTexture(temp_texture);
}

void renderMenu(Player &player) {
	string strs[5] = {
		"Coal: ",
		"Copper: ",
		"Iron: ",
		"Gold: ",
		"Stone: "
	};

	for (short int i = 1; i <= 5; ++i) {
		string msg = strs[i-1] + to_string(player.items[i-1]);

		renderText(
			msg.c_str(),
			{50, 50*i, 200, 200},
			{255, 255, 255, 255}
		);
	}	
}

SaveTile toSaveTile(const Tile &t) {
	
	 SaveTile saveTile = {
		static_cast<uint16_t>(t.sprite.type),
		static_cast<uint16_t>(t.sprite.position.x),
		static_cast<uint16_t>(t.sprite.position.y),
		static_cast<uint16_t>(t.sprite.isShown), 
		static_cast<uint16_t>(t.sprite.hp),
		static_cast<uint16_t>(t.value),
	};
	return saveTile;
}

void loadToDB(std::vector<SaveTile> &buffer) {
	sqlite3* db;
	sqlite3_open(DATA_BASE, &db);
	
	sqlite3_exec(
		db, 
		"CREATE TABLE IF NOT EXISTS maps ("
		"id INTEGER PRIMARY KEY,"
		"width INTEGER,"
		"height INTEGER,"
		"tiles BLOB"
		")",
		nullptr, nullptr, nullptr
	);
	
	sqlite3_stmt *stmt;

	sqlite3_prepare_v2(
		db,
		"INSERT INTO maps (width, height, tiles) VALUES (?, ?, ?)",
		-1, &stmt, nullptr
	);
	
	sqlite3_bind_int(stmt, 1, 512);
	sqlite3_bind_int(stmt, 2, 512);
	sqlite3_bind_blob(
		stmt, 3,
		buffer.data(),
		buffer.size() * sizeof(SaveTile),
		SQLITE_TRANSIENT
	);
	
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	sqlite3_close(db);
}

void pacMap(){
	std::vector<SaveTile> buffer;
	buffer.reserve(GAME_MAP_SIZE * GAME_MAP_SIZE);
	
	for (int x = 0; x < GAME_MAP_SIZE; ++x) {
		for (int y = 0; y < GAME_MAP_SIZE; ++y) {
			buffer.push_back(toSaveTile(game_map[x][y]));
		}
	}
	loadToDB(buffer);	
}

bool Init() {
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		cout << SDL_GetError() << endl;
		return false;
	}
	if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
		cout << IMG_GetError() << endl;
		return false;
	}
	if (TTF_Init() != 0) {
		cout << TTF_GetError() << endl;
		return false;
	}
	
	win = SDL_CreateWindow(
		"Happy Miner",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		SCREEN_SIZE[0],
		SCREEN_SIZE[1],
		SDL_WINDOW_SHOWN	
	);

	if (win == NULL) {
		cout << SDL_GetError() << endl;
		return false;	
	}
	
	ren = SDL_CreateRenderer(
		win,
		-1,
		SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
	);

	if (ren == NULL) {
		cout << SDL_GetError() << endl;
		return false;
	}
	srand(time(NULL));
	return true;
}

void Load() {
	SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor(ren, 0, 0, 0, 128);

	texture_atlas = IMG_LoadTexture(ren, TEXTURE_ATLAS_PATH);

	for (int i = 0; i < GAME_MAP_SIZE; ++i) {
		for (int j = 0; j < GAME_MAP_SIZE; ++j) {
			game_map[i][j] = getRandomTile();
			game_map[i][j].sprite.position = {TILE_SIZE*i, TILE_SIZE*j, TILE_SIZE, TILE_SIZE};
		}
	}
	gameFont = TTF_OpenFont(GAME_FONT_PATH, 15);
}

int main(void) {
	if (!Init()) return 1;
	Load();
	
	SDL_Rect camera;

	camera.h = SCREEN_SIZE[1];
	camera.w = SCREEN_SIZE[0];

	camera.x = (GAME_MAP_SIZE * TILE_SIZE) / 2;
	camera.y = (GAME_MAP_SIZE * TILE_SIZE) / 2;

	Tile* startTile = getTileByScreenPos(SCREEN_SIZE[0] / 2, SCREEN_SIZE[1] / 2, camera);
	startTile->sprite.isShown = true;
	startTile->sprite.type = VOID;

	Player player;
	player.sprite.type = PLAYER;	
	player.sprite.position.x = SCREEN_SIZE[0] / 2 + 10;
	player.sprite.position.y = SCREEN_SIZE[1] / 2 + 10;

	player.sprite.position.h = TILE_SIZE / 2;
	player.sprite.position.w = TILE_SIZE / 2;

	player.items = new short int[5];
	for (int i = 0; i < 5; ++i) player.items[i] = 0;
	
	short unsigned int 
		map_w = GAME_MAP_SIZE * TILE_SIZE,
		map_h = GAME_MAP_SIZE * TILE_SIZE;

	bool isDone = false;
	SDL_Event event;

	while (!isDone) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT) {
				isDone = true;
			}
			else if (event.type == SDL_MOUSEBUTTONDOWN) {
				changeToVoid(camera, player);
			}
		}
          
		const Uint8* keys = SDL_GetKeyboardState(NULL);	

		if (keys[SDL_SCANCODE_W]) {
			cameraMovement(player, camera, 0, -CAMERA_SPEED);
			//camera.y -= CAMERA_SPEED;
		} 
		if (keys[SDL_SCANCODE_S]) {
			cameraMovement(player, camera, 0, CAMERA_SPEED);
			//camera.y += CAMERA_SPEED;
		}
		if (keys[SDL_SCANCODE_A]) {
			cameraMovement(player, camera, -CAMERA_SPEED, 0);
			//camera.x -= CAMERA_SPEED;
		}
		if (keys[SDL_SCANCODE_D]) {
			cameraMovement(player, camera, CAMERA_SPEED, 0);			
			//camera.x += CAMERA_SPEED;
		}

		if (camera.x < 0) camera.x = 0;
		if (camera.y < 0) camera.y = 0;

		if (camera.x > map_w - camera.w)
    	camera.x = map_w - camera.w;

		if (camera.y > map_h - camera.h)
    	camera.y = map_h - camera.h;
		
		SDL_RenderClear(ren);
		
		renderTile(game_map, camera);

		renderSellected(player, camera);
		
		renderPlayer(player);
	
		renderMenu(player);	
	
		SDL_RenderPresent(ren);
		SDL_SetRenderDrawColor(ren, 0, 0, 0, 128);
		SDL_Delay(30);
	}
	
	pacMap();
	
	SDL_DestroyTexture(texture_atlas);
	SDL_DestroyRenderer(ren);

	SDL_Quit();
	IMG_Quit();
	
	delete[] player.items;
	return 0;
}
