# Tic-Tac-Toe-Server
Implements an on-line multiplayer Tic-Tac-Toe game service. The two main programs include ttt.c which implements a simple game client
and ttts.c the server used to coordinate games and enforce the rules.  

Steps to run server and game with general clients: 

1) Compile all programs by simply typing “make” into terminal. Make sure you are in the correct directory. 
2) In order to run a minimum of one game, first start the server. The server program takes in one argument which is the port number to which connections should be made on. To set this up and 
  get the server running, type into terminal “./ttts port_number” where the port number should be a large number. It is suggested to use a number such as 15000 as it is unlikely this port is 
  taken. To connect two clients to make one game, first start two more terminals that is in the same directory as all the files. Then type into each terminal “./ttt domain_name port_number” 
  where the two arguments it takes in is the domain name and port number. If running on local host, type in localhost and the port number is the same port number used to start the server. 
  In response to the first connection, the client should receive a wait message. Once the second client connects, both players should receive a start game message. 
  
Messages sent by clients must be formatted in a certain manner: 
---------------------------------------------------------------
Messages are broken into fields. A field consists of one or more characters, followed by a vertical bar.
The first field is always a four-character code. The second field gives the length of the remaining message in bytes, represented as string containing a decimal integer in the range 0–255. 
This length does not include the bar following the size field, so a message with no additional fields, such as WAIT, will give its size as 0. 
Subsequent fields are variable-length strings. Note that a message will always end with a vertical bar. 

The field types are:
name: Arbitrary text representing a player’s name.
role: Either X or O.
position: Two integers (1, 2, or 3) separated by a comma.
board: Nine characters representing the current state of the grid, using a period (.) for unclaimed
grid: cells, and X or O for claimed cells.
reason: Arbitrary text giving why a move was rejected or the game has ended.
message: One of S (suggest), A (accept), or R (reject).
outcome: One of W (win), L (loss), or D (draw).

Messages sent by client:
------------------------
PLAY Sent once a connection is established. The third field gives the name of the player.
The expected response from the server is WAIT. The server will respond INVL if the name
cannot be used (e.g., is too long).

MOVE Indicates a move made by a player. The third field is the player’s role and the fourth field
is the grid cell they are claiming.
The server will respond with MOVD if the move is accepted or INVL if the move is not allowed.

RSGN Indicates that the player has resigned.
The server will respond with OVER.

DRAW Depending on the message, this indicates that the player is suggesting a draw (S), or is
accepting (A) or rejecting (R) a draw proposed by their opponent.
Note that DRAW A or DRAW R can only be sent in response to receiving a DRAW S from the
server.

Messages sent by server: 
------------------------
BEGN Indicates that play is ready to begin. The third field is the role (X or O) assigned to the
player receiving the message and the fourth field is the name of their opponent.
If the role is X, the client will respond with MOVE, RSNG, or DRAW. Otherwise, the client will
wait for MOVD.

MOVD Sent to both participants to indicate that a move has occurred. The third field is the role
of the player making the move and the fourth field gives the current state of the grid.
The player that made the move will wait for their opponent’s move. The opponent client will
respond with MOVE, RSGN, or DRAW.

INVL Indicates that the client’s message was invalid. The third field is arbitrary text explaining
why the message was rejected. INVL is both to reject illegal moves and to report protocol errors.
When used to reject a protocol error (such as a message being sent at an inappropriate time), the
explanation must begin with an exclamation point (!).

DRAW If one player suggests a draw, the server will send a draw suggestion to their opponent.
The expected response is DRAW A or DRAW R.
After receiving DRAW A, the server ends the game and sends OVER to both clients.

After receiving DRAW R, the server sends DRAW R to the player that had sent DRAW S.
OVER Indicates that the game has ended. The second field indicates whether the recipient has
won (W), lost (L), or drawn (D). The third field is arbitrary text explaining the outcome (e.g., one
player has completed a line, someone has resigned, the grid is full).



