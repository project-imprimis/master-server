This is the master server code for the Imprimis engine, seperated into its own repository.
The code that was used to create this server was formerly part of the game itself;
it has been moved to its own repository due to its lack of overlap with the client.

The Imprimis master server allows for a centralized authentication and directory service
that can tell clients what servers are located at what addresses and authenticate players by
use of a private/public key combination.

The master server's only dependency is the enet library included in the `enet` submodule.
Due to changes between the engine's copy of enet and other versions, do not link this program
with other installations of the `enet` library. Other dependencies, like libSDL2, required for
the client are not required for the master server.
