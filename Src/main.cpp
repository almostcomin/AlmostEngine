#include <SDL3/SDL_main.h>
#include <SDL3/SDL_init.h>

int SDL_main(int argc, char* argv[]) 
{
	// Your SDL application code goes here
	// For example, initializing SDL and creating a window
	// Initialize SDL
	if (SDL_Init(SDL_INIT_VIDEO) < 0) 
	{
		return -1; // Initialization failed
	}

	// Create a window
	SDL_Window* window = SDL_CreateWindow("SDL Application", 800, 600, SDL_WINDOW_RESIZABLE);
	if (!window) 
	{
		SDL_Quit();
		return -1; // Window creation failed
	}

	// Main loop (placeholder)
	bool running = true;
	while (running) 
	{
		SDL_Event event;
		while (SDL_PollEvent(&event)) 
		{
			if (event.type == SDL_EVENT_QUIT)
			{
				running = false;
			}
		}
		// Render or update logic would go here
	}

	// Clean up
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0; // Exit successfully
}