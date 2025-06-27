#ifndef CDG_PLAYER_H
#define CDG_PLAYER_H

/**
 * @brief Starts the CD+G player for the given filename.
 * 
 * This function reads a .cdg file from the filesystem, parses it, and
 * renders the graphics to the text-mode screen. The player will run
 * until the file ends or the user presses the ESC key.
 * 
 * @param filename The name of the .cdg file to play.
 */
void cdg_player_start(const char* filename);

#endif // CDG_PLAYER_H