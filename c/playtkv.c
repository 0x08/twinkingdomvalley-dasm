#include <ctype.h>
#include <stdarg.h>
#include "libtkv.h"

#define NLOCATIONS 0xB1 //Locations $00-$B0, see the tables at $2A00 and $2B00
#define MAXLINE 1000

#define QUIT -1
#define EMPTY -2
#define INVALID_MOVE 0xFF
#define SUCCESS 0
#define CONTINUE 1
#define MOVED 2
#define LOOK 3
#define DRAW 4

#define NOMATCH 0xFF
#define MAXMATCHES 10

#define PASSABLE 1
#define IMPASSABLE 0
#define NORMAL 0
#define NONDOOR 1
#define NONDOOR_SEETHRU 2
#define DOOR 3
#define UNLOCKED 0
#define LOCKED 1

#define UNLOCK 0
#define LOCK 1

#define NONHOSTILE 0
#define HOSTILE 1

#define LAMP 0x01
#define JUG 0x09
#define FLINT 0x0A
//Codes the jug ($2589) can have
#define JUG_EMPTY 0x0A
#define JUG_WATER 0x0B
#define JUG_OIL 0x0C
#define JUG_GOLD 0x1B
#define JUG_BEER 0x1C

#define DARK 1
#define LIGHT 0

//Object at location CARRED+id is carried by character id
#define CARRIED 0xC8 

struct location {
  UCHAR id;
  char *description;
  UCHAR nexits;
  UCHAR locationtype;
  struct exit *exits;
  UCHAR isdark;
  UCHAR islongdesc;
  UCHAR isvisited;
};

struct exit {
  UCHAR direction; //first&0x3F - bits 6 and 7 info in ispassable etc
  char *dirtext;
  char *exittext; //This will be null for normal exits
  UCHAR destination;
  UCHAR ispassable;
  UCHAR exittype;
  UCHAR islocked;
  long addr; //Offset of the exit's first byte in ctkv
};

struct object {
  char *description;
  UCHAR location_id;
  UCHAR code; //$2580: indexes the $2600-$2726 property tables
  UCHAR size;
  UCHAR strength_required;
  UCHAR durability;
  UCHAR max_throw_damage;
  UCHAR rnd_throw_damage;
  UCHAR max_melee_damage;
  UCHAR rnd_melee_damage;
};

struct character {
  char *description;
  UCHAR location_id;
  UCHAR classcode; //Raw $2968 byte: bit 7 set means hostile to the player
  UCHAR class;     //classcode&0x1F, indexes the $2800-$28E0 class tables
  UCHAR ishostile;
  UCHAR strength;
  UCHAR max_carried;
  UCHAR amount_carried;
};

void init_game();

void process_output(char *s);
int read_command(UCHAR *matches, int *nmatches);
int get_command(UCHAR *matches, int *nmatches);
void free_action(UCHAR code);
void player_move(UCHAR *matches, int nmatches);
void turn_start();
void turn_end();
void npcs_act(int here);
void npc_act(UCHAR x);
void npc_turn(UCHAR x);
UCHAR rnd();
void sentence(const char *fmt, ...);
const char * is_are(UCHAR id);
UCHAR carrier_location(UCHAR objid);
int transfer(UCHAR objid, UCHAR dest, UCHAR flag);
int weapon_too_weak(UCHAR objid);
int melee_attack(UCHAR objid, UCHAR victim, UCHAR flag);
int throw_attack(UCHAR objid, UCHAR victim, UCHAR flag);
void weapon_lands(UCHAR objid, UCHAR flag);
void inflict(UCHAR victim, UCHAR max, UCHAR rndmask);
void damage(UCHAR victim, UCHAR amount);
void print_score_and_strength();
void player_attack(UCHAR verb, UCHAR *matches, int nmatches);
UCHAR get_match(UCHAR *word);
void report_invalid_move(struct exit *e, UCHAR dirindex, UCHAR type);

int noncombat_action(UCHAR code, UCHAR *matches, int nmatches);
int use_lamp(UCHAR action);
int use_key(UCHAR action, UCHAR *matches, int nmatches);
void use_key_on_doors(struct exit *e);
int player_has_key(char *doortext);
int drop(UCHAR verb, UCHAR *matches, int nmatches);
int take(UCHAR *matches, int nmatches);
int inventory(UCHAR character_id);
int print_carried(UCHAR loc, int holdall_follows);
int yes_no();
void fill_holdall();
void empty_holdall();
int fill(UCHAR verb, UCHAR *matches, int nmatches);
int pour(UCHAR verb, UCHAR *matches, int nmatches);
int drink();
int swim();
int give(UCHAR verb, UCHAR *matches, int nmatches);
int ask(UCHAR *matches, int nmatches);
int given(UCHAR id);
int wave(UCHAR *matches, int nmatches);
void help();
int move(UCHAR dir, UCHAR dirindex);

char * create_copy_string(char *from);
char * sfirstcaps(char *s);

void load_location(struct location *l, UCHAR id);
void load_object(struct object *o, UCHAR id);
void set_object_code(struct object *o, UCHAR code);
void reload_location(UCHAR id);
int player_has(UCHAR objid);
int lamp_is_lit();
void move_player(UCHAR id);
char * word_of(UCHAR code);
int ask_carried(UCHAR carrier, UCHAR prompt, UCHAR *matches, int *nmatches, UCHAR *id);
int count_word(UCHAR w, UCHAR *matches, int nmatches);
int match_character(UCHAR loc, UCHAR *matches, int nmatches, UCHAR *id);
int match_object_words(UCHAR loc, UCHAR *matches, int nmatches, UCHAR *id);
int match_carried_object(UCHAR carrier, UCHAR *matches, int nmatches, UCHAR *id);
void ask_again(UCHAR word, UCHAR *matches, int *nmatches);
void load_character(struct character *c, UCHAR id);
long load_exit(struct exit *e, long addr);
void free_location(struct location *l);
void free_object(struct object *l);
void free_character(struct character *c);
void cleanup();

char * get_dirtext(UCHAR dirbyte);
void print_short_description(UCHAR i);
void print_long_description(UCHAR i);
void print_description(UCHAR i);
void print_objects(UCHAR i);
void update();
void print_npc_info();
char * get_nondoor_text(UCHAR firstbyte, UCHAR thirdbyte);
char * get_door_text(UCHAR firstbyte, UCHAR thirdbyte);
int print_word_or_zero(char *ss, int pos, long addr, UCHAR byte);
void you_can_see(char *text, struct location dest);

struct character * get_npc(UCHAR id);
struct location * get_location(UCHAR i);
struct character * get_player();
struct location * get_player_location();
void set_player_location_id(UCHAR i);
UCHAR get_player_location_id();
struct object * get_object(UCHAR i);

//Never access these global variables directly - use get/set methods
//Exception is during loading data and freeing location info
struct location *locations;
struct object *objects;
struct character *characters;

UCHAR lampoil; //Stored in $27FB

int debug=0; //Set by -d: show player location, lamp oil and character moves

//Per-turn state, named after the original's memory locations
#define MAXCHARS 0x26 //$0C1E clears $2AD0-$2AF5
UCHAR status[MAXCHARS]; //$2AD0: bit 7 attacked, bit 6 moved after combat/player moving, bit 5 moved
UCHAR loc14; //$14: player location at the start of this turn
UCHAR loc1c; //$1C: player location at the start of the previous turn
UCHAR nextloc[MAXCHARS]; //$2BD0: where each character has decided to move

//Working storage for character actions, named after the zero page bytes
UCHAR rnd1d=0xAA,rnd1e=0,rnd1f=0; //$1D-$1F: random number state, $1D set at $1403
UCHAR pass15;   //$15: bit 7 set when actions are carried out, clear when only decided
UCHAR npc16;    //$16: the character whose turn it is
UCHAR target17; //$17: healthiest enemy found by find_enemy()
UCHAR at1a;     //$1A: where the action is happening
UCHAR align1b;  //$1B: $2968 byte of the acting character, bit 7 is its side
UCHAR prop18;   //$18: $2880 class entry: bits 4-7 allowed regions, bits 0-3 chance of wandering
UCHAR prop19;   //$19: $28A0 class entry: non-zero means it follows the player
UCHAR prev13;   //$13: status of the acting character at the start of its turn
UCHAR result10; //$10: non-zero if the character did something
signed char staff27=2; //$27: the staff drops once this goes negative
UCHAR holdallweight=0; //$27FA
UCHAR thirst02=0x80; //$02: goes down each turn, faster in the desert; drinking resets it
UCHAR magic00=0;     //$00: bit 7 set once the concealed doors are revealed
#define BROKEN 0xC7 //Location of a broken weapon or a dead character
#define FIRSTWEAPON 0x0A //Objects $0A-$22 can be used as weapons
#define ENDWEAPON 0x23
#define STAFF 0x16 //Object code of the long wooden staff

//Command codes, see the $3600 word table and $1EB1 dispatch table
#define FIRSTCOMBAT 0x0E //HIT, CUT, THROW are $0E-$10
#define FIRSTACTION 0x11 //$11-$24 cost a turn
#define FIRSTFREE 0x25   //$25-$2E are carried out without a turn passing, see $0C8E
#define FIRSTNOUN 0x2F   //$2F onwards are objects, characters and places
int verb83; //$83: the first verb-like word of the last input, or -1

int main(int argc, char *argv[]) {
  int i;
  if(argc>1 && strcmp(argv[1],"-d")==0){
    debug=1;
    argc--;
    argv++;
  }
  init_tkv(); //Loads original TKV binary and sets up lookup addresses
  init_game();
  locations = (struct location *) malloc(NLOCATIONS*sizeof(struct location));
  for(i=0;i<NLOCATIONS;i++)
    load_location(locations+i,i);
  objects = (struct object *) malloc(getnumberofobjects()*sizeof(struct object));
  for(i=0;i<getnumberofobjects();i++)
    load_object(objects+i,i);
  characters = (struct character *) malloc(getnumberofcharacters()*sizeof(struct character));
  for(i=0;i<getnumberofcharacters();i++)
    load_character(characters+i,i);

  if(argc>1){
    print_description(strtol(argv[1],NULL,16));
    return 0;
  }

  set_player_location_id(1);
  print_description(get_player_location_id());
  //Main game loop, see $1451
  for(;;){
    UCHAR matches[MAXMATCHES];
    int nmatches;
    turn_start();
    int verb=get_command(matches,&nmatches);
    if(verb==QUIT)
      break;
    npcs_act(0);//Characters away from the player act, $0C98
    if(verb<FIRSTCOMBAT){//A direction
      if(status[0]&0x80){//Player attacked last turn so gets away first, $145D
        player_move(matches,nmatches);
        status[0]=0x40;
        turn_end();
      } else {//Characters here act before the player moves, $1474
        turn_end();
        player_move(matches,nmatches);
      }
      status[0]=0;
    } else if(verb<FIRSTACTION){//Combat, see $147F
      player_attack(verb,matches,nmatches);
      turn_end();
    } else {
      noncombat_action(verb,matches,nmatches);
      turn_end();
      status[0]=0;
    }
  }

  cleanup();
  return 0;
}

void init_game(){
  lampoil=0x80; //See code just after $1400
}

//Start of a turn: the first part of $0C00 with A=0
void turn_start(){
  int x;
  //Turn separator: the original draws a line of magenta dashes
  if(debug)
    printf("          ---------------%02x,%02x\n",get_player()->location_id,lampoil);
  else
    printf("          ---------------\n");
  update();
  for(x=0;x<MAXCHARS;x++)//$0C1E: forget all but last turn's attacks
    status[x]&=0x80;
  pass15=0;//Characters decide what to do but nothing happens yet, $0C2D
  for(x=getnumberofcharacters()-1;x>0;x--)
    npc_act(x);
  print_npc_info();
}

//With A=$80, $0C00 lets the characters act: those away from the player
//before the player's command ($0C98), those with the player after it
//($0CAE). Then everyone who decided to move does so ($0CC3).
void npcs_act(int here){
  int x;
  pass15=0x80;
  for(x=getnumberofcharacters()-1;x>0;x--)
    if( (get_npc(x)->location_id==loc14) == here )
      if( (status[0]&0x40)==0 )//$0CD9: not if the player is getting away
        npc_act(x);
}

//End of a turn: $0C00 with A=$80
void turn_end(){
  int x;
  npcs_act(1);
  for(x=getnumberofcharacters()-1;x>0;x--){//$0CC3
    struct character *c=get_npc(x);
    if(c->strength!=0 && (status[x]&0x60)!=0){
      if(debug)
        printf("[%02x %s: %02x -> %02x]\n",x,c->description,c->location_id,nextloc[x]);
      c->location_id=nextloc[x];
    }
  }
}

//Per-turn events, see $0F03
void update(){
  int x;
  //$0F4A: every living character, the player included, recovers some health
  for(x=getnumberofcharacters()-1;x>=0;x--){
    struct character *c=get_npc(x);
    if(c->strength==0)
      continue;
    UCHAR max=getbyte("2800",c->class);
    UCHAR h=c->strength+getbyte("28C0",c->class);
    c->strength = h>=max ? max : h;
  }
  //$0F6A: a lit lamp burns oil, flickering when low and going out when empty
  if(lamp_is_lit()){
    struct object *lamp=get_object(LAMP);
    if(--lampoil==0){
      set_object_code(lamp,1);
      process_output("Your lamp dies.\n");
    } else if(lampoil==0x14 && (lamp->location_id==CARRIED || lamp->location_id==get_player_location_id()))
      process_output("Your lamp flickers.\n");
  }
  //$0FAE: thirst, faster in the desert ($20-$2F and $80-$84)
  UCHAR here=get_player_location_id();
  UCHAR rate = ( (here>=0x20 && here<0x30) || (here>=0x80 && here<0x85) ) ? 7 : 1;
  if(rate<thirst02){
    thirst02-=rate;
    if(thirst02<0x28)
      process_output("You are very dry.\n");
  } else {
    if(thirst02!=0){
      thirst02=0;
      process_output("You are ill with thirst.\n");
    }
    at1a=here;
    damage(0,rate*4);
  }
  //$0FFE: remember where the player has been
  loc1c=loc14;
  loc14=get_player_location_id();
}

void process_output(char *s){
  printf("%s",s);
}

//Asks for input until a command that costs a turn is given, see $0C78
//Returns the first verb-like command ($83) or QUIT
int get_command(UCHAR *matches, int *nmatches){
  for(;;){
    process_output("?");
    int r=read_command(matches,nmatches);
    if(r==QUIT)
      return QUIT;
    if(r==EMPTY)
      continue;
    int verb=verb83;
    if(verb<0){
      process_output("I don't understand.\n");
      continue;
    }
    if(verb<FIRSTFREE)
      return verb;
    if(verb==0x25 || verb==0x26)//QUIT,END
      return QUIT;
    free_action(verb);
  }
}

//Reads a line of input and converts its words to command codes, see $229F
//Returns QUIT at end of input, EMPTY if there's nothing to act on
int read_command(UCHAR *matches, int *nmatches){
  char line[MAXLINE];
  int c,i=0;

  *nmatches=0;
  verb83=-1;
  do {
    c=getchar();
    line[i++]=c;
  } while( i<MAXLINE && c!=EOF && c!='\n' );
  line[i-1]='\0';
  int len=i;

  if(c==EOF && i==1){//End of input with nothing left to process
    process_output("\n");
    return QUIT;
  }
  if(i==1)
    return EMPTY;
  for(i=0;i<len-1;i++){//$230D loop
    if(line[i]=='*'){//$2310
      process_output("The * commands are not implemented yet.\n");
      return EMPTY;
    }
    char word[MAXWORD];
    int j=0;
    while(isalpha((unsigned char)line[i]) && i<len-1){//$231E, letters checked at $238D
      if(j<MAXWORD-1)//Letters beyond the buffer are dropped
        word[j++]=tolower((unsigned char)line[i]);
      i++;
    }
    word[j]='\0';
    if(j==0)//Not a letter: an empty word would match the first table entry, NORTH
      continue;
    UCHAR m=get_match(word);
    //printf("Match result for %s: %02x\n",word,m);
    if(m==NOMATCH)
      continue;
    if(m<FIRSTNOUN && verb83<0)//$2378: the first verb-like word is the command
      verb83=m;
    if(m>=FIRSTCOMBAT && m<FIRSTNOUN)//$2384: verbs aren't matched against objects
      m=0xFF;
    if(*nmatches<MAXMATCHES)
      matches[(*nmatches)++]=m;
  }
  return SUCCESS;
}

//Combines the direction words given and tries to move, see $23C3
void player_move(UCHAR *matches, int nmatches){
  //These dirbytes are in $23B5-C3
  //Correspond to: N  S  E  W  U  D  NE NW NE NW SE SW SE SW
  UCHAR dirbytes[]={0x20, 0x10, 0x08, 0x04, 0x02, 0x01,
    0x28, 0x24, 0x28, 0x24, 0x18, 0x14, 0x18, 0x14};
  int i;
  UCHAR dir=0;
  for(i=0;i<nmatches;i++)//See $23CA
    if(matches[i]<FIRSTCOMBAT)
      dir=dir|dirbytes[matches[i]]; //Set bits for commanded direction
  for(i=0;i<0x0E;i++)   //Find index of commanded direction
    if(dir==dirbytes[i])
      break;
  if(move(dir,i)==MOVED)
    print_description(get_player_location_id());
}

//Commands $25-$2E don't use up a turn
void free_action(UCHAR code){
  switch(code){
    case 0x27: case 0x28: print_description(get_player_location_id()); return; //LOOK,VIEW
    case 0x29: case 0x2A: process_output("Apologies, graphics not implemented yet.\n"); return; //PICTURE,DRAW
    case 0x2B: print_score_and_strength(); return; //SCORE, $12EE
    case 0x2C: inventory(0); return; //INVENTORY
    case 0x2D: process_output("Not implemented yet.\n"); return; //OPTION
    case 0x2E: help(); return; //HELP
  }
}

//Called with code between 0x11 and 0x24
int noncombat_action(UCHAR code, UCHAR *matches, int nmatches){
  //printf("noncombat_action: %02x.\n",code);
  switch(code){ //code is 0x11 to 0x24
    case 0x11: return drop(code,matches,nmatches); //DROP
    case 0x12: case 0x13: return take(matches,nmatches); //TAKE,GET
    case 0x14: case 0x15: case 0x16: return use_key(LOCK,matches,nmatches); //LOCK,SHUT,CLOSE
    case 0x17: case 0x18: return use_key(UNLOCK,matches,nmatches); //OPEN,UNLOCK
    case 0x19: case 0x1a: return use_lamp(LIGHT); //LIGHT,ON
    case 0x1b:            return use_lamp(DARK);  //OFF    
    case 0x1d: return fill(code,matches,nmatches); //FILL
    case 0x1e: case 0x1f: return pour(code,matches,nmatches); //POUR,EMPTY
    case 0x1c: return give(code,matches,nmatches); //GIVE
    case 0x20: return ask(matches,nmatches); //ASK
    case 0x21:            process_output("You wait here.\n"); return CONTINUE; //WAIT
    case 0x22: return wave(matches,nmatches); //WAVE
    case 0x23: return swim(); //SWIM
    case 0x24: return drink(); //DRINK
  }
  fprintf(stderr,"This should never be printed.\n");
  return EMPTY; //Should never get here
}

//LIGHT and ON ($16A7), OFF ($16F9). The lamp is lit when its code is 0.
int use_lamp(UCHAR action){
  struct object *lamp = get_object(LAMP);
  if(!player_has(LAMP)){
    process_output("You don't have a lamp.\n");
    return CONTINUE;
  }
  if(action==LIGHT){
    if(lamp_is_lit())
      process_output("Your lamp is already on.\n");
    else if(!player_has(FLINT))
      process_output("You don't have a flint.\n");
    else if(lampoil==0)
      process_output("Your lamp is empty.\n");
    else {
      set_object_code(lamp,0);
      process_output("The lamp is now on.\n");
    }
  } else {
    if(!lamp_is_lit())
      process_output("Your lamp is already off.\n");
    else {
      set_object_code(lamp,1);
      process_output("The lamp is now off.\n");
    }
  }
  return SUCCESS;
}

int use_key(UCHAR action, UCHAR *matches, int nmatches){
  struct location *pl = get_player_location();
  int i;
  for(i=0;i<pl->nexits;i++){
    struct exit *e = pl->exits+i;
    if(e->exittype==DOOR){//The grate is classed as a DOOR
      if(action==UNLOCK && e->islocked==UNLOCKED){//$17AD
	process_output("It's already open.\n");
	return CONTINUE;
      } else if(action==LOCK && e->islocked==LOCKED){//$17C6
	process_output("It's already locked.\n");
	return CONTINUE;
      } else if(player_has_key(e->exittext)==0){//$1790
	process_output("You don't have the key.\n");
	return CONTINUE;
      } else  {//Player has the right key for the exit
	use_key_on_doors(e);
	return SUCCESS;
      }
    }
  }
  //Will only get here if no lockable exit (DOOR) is found in loop
  process_output("Nothing here locks.\n"); //$1733
  return CONTINUE;
}

//Unlocks a d
void use_key_on_doors(struct exit *e){
  struct location *dest = get_location(e->destination);
  struct exit *otherside=NULL;
  int i;
  
  //printf("otherside: %s\n",dest->description);
  for(i=0;i<dest->nexits;i++){
    otherside=dest->exits+i;
    //printf("|%s| |%s|\n",otherside->exittext, e->exittext);
    if( otherside->exittext!=NULL && strcmp(otherside->exittext, e->exittext)==0 )
      break;
  }
  if(otherside==NULL)
    fprintf(stderr,"SERIOUS ERROR: %s is one sided at location id %02x.\n",e->exittext,e->destination);
  ctkv[e->addr]^=0x40; //$1764: the original flips bit 6 in the location data
  ctkv[otherside->addr]^=0x40;
  if(e->islocked==LOCKED){ //NEED TO ALTER DESCRIPTION TEXT AND PASSABLE
    e->islocked=UNLOCKED;
    e->ispassable=PASSABLE;
    otherside->islocked=UNLOCKED;
    otherside->ispassable=PASSABLE;
    process_output("It is now opened.\n"); //Just before $178D
  } else {
    e->islocked=LOCKED;
    e->ispassable=IMPASSABLE;
    otherside->islocked=LOCKED;
    otherside->ispassable=IMPASSABLE;
    process_output("It is now locked.\n");
  }
}

int player_has_key(char *doortext){
  UCHAR id;
  for(id=0;id<getnumberofobjects();id++){
    struct object *o=get_object(id);
    if(o->location_id==CARRIED){//Player has this object
      char desc[100];
      char *key;
      strcpy(desc,o->description);
      key= strstr(desc,"key");
      //printf("|%s| |%s|\n",key,doortext);
      if(key!=NULL){
	char *keytype=desc+2;//Skip "A " to point to "brass" etc
	(key-1)[0]='\0'; //Overwrite space before "key";
	if(keytype[0]=='m' //Must be the master key
	    || strstr(doortext,"metal")!=NULL //Any key matches the metal door
	    || strstr(doortext,keytype)!=NULL){ //Key type matches door eg brass, bronze etc
	  return 1;
	}
      }
    }
  }
  return 0;//Player doesn't have key
}

//Lists what is at loc: the following objects, or nothing. With
//holdall_follows set, says nothing when there are none. See $164C.
//Returns the number of objects listed.
int print_carried(UCHAR loc, int holdall_follows){
  int id,nitems=0;
  for(id=getnumberofobjects()-1;id>0;id--){
    struct object *obj=get_object(id);
    if(obj->location_id==loc){
      if(nitems++==0)
        process_output("the following\n");
      char line[MAXLINE];
      snprintf(line,sizeof(line),"  %s\n",obj->description);
      line[2]=toupper(line[2]);
      process_output(line);
    }
  }
  if(nitems==0 && !holdall_follows)
    process_output("nothing.\n");
  return nitems;
}

//INVENTORY ($162F) for the player, or what a character here has ($0C5E)
int inventory(UCHAR character_id){
  UCHAR loc=CARRIED+character_id;
  int hasholdall = get_object(0)->location_id==loc;
  process_output(character_id==0 ? "You have " : " is here with ");
  int n=print_carried(loc,hasholdall);
  if(hasholdall){//$163F
    process_output(n ? "A holdall containing " : "a holdall containing ");
    print_carried(0xFF,0);
  }
  return EMPTY;
}

//TAKE and GET, see $1548
int take(UCHAR *matches, int nmatches){
  UCHAR id;
  UCHAR here=get_player_location_id();
  if(match_object_words(here,matches,nmatches,&id)==0){
    process_output("Take what?");
    if(read_command(matches,&nmatches)!=SUCCESS)
      nmatches=0;
    if(match_object_words(here,matches,nmatches,&id)==0){
      process_output("I can't see any here.\n");
      return EMPTY;
    }
  }
  if(transfer(id,CARRIED,0xFF)){//$1597
    process_output("You can't carry any more.\n");
    return CONTINUE;
  }
  process_output("I have it now.\n");
  return SUCCESS;
}

//DROP, see $160F. Objects in a carried holdall can be dropped directly.
int drop(UCHAR verb, UCHAR *matches, int nmatches){
  UCHAR id;
  if(ask_carried(CARRIED,verb,matches,&nmatches,&id)==0){
    process_output("You haven't got that.\n");
    return EMPTY;
  }
  transfer(id,get_player_location_id(),0xFF);
  process_output("OK.\n");
  return SUCCESS;
}

//Reads a yes or no answer, see $1A3A
int yes_no(){
  int c;
  process_output(" (yes / no) ?");
  for(;;){
    c=getchar();
    if(c==EOF)
      return 0;
    if(c=='y' || c=='Y' || c=='n' || c=='N'){
      int answer = c=='y' || c=='Y';
      while(c!='\n' && c!=EOF)
        c=getchar();
      process_output("\n");
      return answer;
    }
  }
}

//FILL HOLDALL: offers each object carried, see $1A4E
void fill_holdall(){
  int id;
  process_output("You have ");
  print_carried(CARRIED,0);
  for(id=getnumberofobjects()-1;id>0;id--){
    struct object *obj=get_object(id);
    if(obj->location_id!=CARRIED)
      continue;
    sentence("Put %s in",obj->description);
    if(yes_no() && transfer(id,0xFF,0xFF))
      sentence("You can't get %s in.\n",obj->description);
  }
}

//EMPTY or POUR HOLDALL: everything in it falls out here, see $19D4
void empty_holdall(){
  int id;
  process_output("OK.\n");
  for(id=getnumberofobjects()-1;id>0;id--)
    if(get_object(id)->location_id==0xFF)
      get_object(id)->location_id=get_player_location_id();
  holdallweight=0;
}

//Is the player by water? See $1A16 and the table at $1A24.
int at_water(){
  int i;
  for(i=0;i<0x0F;i++)
    if(getbyte("1A24",i)==get_player_location_id())
      return 1;
  return 0;
}

//Is the water deep enough to swim in? See $1A08 and the table at $1A2F.
int at_deep_water(){
  int i;
  for(i=0;i<4;i++)
    if(getbyte("1A2F",i)==get_player_location_id())
      return 1;
  return 0;
}

//The jug's contents are gone and it weighs $0F less, see $192E
void empty_jug(){
  struct object *jug=get_object(JUG);
  if(jug->location_id==CARRIED)
    get_player()->amount_carried-=0x0F;
  else
    holdallweight-=0x0F;
  set_object_code(jug,JUG_EMPTY);
}

//"You can't fill a brass key", see $1ABE
void cant(UCHAR verb, UCHAR id){
  sentence("You can't %s %s.\n",word_of(verb),get_object(id)->description);
}

//FILL, see $1AAC
int fill(UCHAR verb, UCHAR *matches, int nmatches){
  UCHAR id;
  UCHAR here=get_player_location_id();
  if(ask_carried(CARRIED,verb,matches,&nmatches,&id)==0){
    process_output("You haven't got that.\n");
    return EMPTY;
  }
  if(id==0){
    fill_holdall();
  } else if(id==LAMP){//$1AD1
    if(at_water())
      process_output("Don't be silly!\n");
    else if(here==0x3C || (player_has(JUG) && get_object(JUG)->code==JUG_OIL)){
      if(here!=0x3C)
        empty_jug();
      lampoil=0xFF;
      process_output("OK.\n");
    } else
      process_output("There is no oil here.\n");
  } else if(id==JUG){//$1B1A
    UCHAR code;
    struct object *jug=get_object(JUG);
    if(jug->code!=JUG_EMPTY){
      process_output("It's already full.\n");
      return SUCCESS;
    }
    if(at_water())
      code=JUG_WATER;
    else if(here==0xAE)
      code=JUG_GOLD;
    else if(here==0x3F || here==0x9D)
      code=JUG_BEER;
    else if(here==0x3C)
      code=JUG_OIL;
    else {
      process_output("There is no liquid here.\n");
      return SUCCESS;
    }
    if(jug->location_id==CARRIED){//$1588
      int w=get_player()->amount_carried+0x0F;
      if(w>0xFF || w>=get_player()->max_carried){
        process_output("You can't carry any more.\n");
        return SUCCESS;
      }
      get_player()->amount_carried=w;
    } else {//In the holdall
      if(holdallweight+0x0F>=0x64){
        process_output("You can't carry any more.\n");
        return SUCCESS;
      }
      holdallweight+=0x0F;
    }
    set_object_code(jug,code);
    process_output("OK.\n");
  } else
    cant(0x1D,id);
  return SUCCESS;
}

//POUR and EMPTY, see $19A2
int pour(UCHAR verb, UCHAR *matches, int nmatches){
  UCHAR id;
  if(ask_carried(CARRIED,verb,matches,&nmatches,&id)==0){
    process_output("You haven't got that.\n");
    return EMPTY;
  }
  if(id==0)
    empty_holdall();
  else if(id==LAMP)
    process_output("Don't be silly!\n");
  else if(id==JUG){
    if(get_object(JUG)->code==JUG_EMPTY)
      process_output("It's already empty.\n");
    else {
      process_output("The liquid drains away.\n");
      empty_jug();
    }
  } else
    cant(verb,id);
  return SUCCESS;
}

//The strong beer at the inn or in the jug, see $18F7
void drink_beer(){
  process_output("The strong beer refreshes you.\n");
  inflict(0,0x32,0x1F);
  thirst02=0xFF;
}

//Seven locations gain a hidden exit, stored after their others, see $1973
void reveal_concealed_doors(){
  int i;
  for(i=6;i>=0;i--){
    UCHAR loc=getbyte("1A33",i);
    ctkv[getlocationaddress(loc)+1]+=0x20;//One more exit
    reload_location(loc);
  }
}

//DRINK, see $1950
int drink(){
  UCHAR here=get_player_location_id();
  struct object *jug=get_object(JUG);
  if(at_water()){
    thirst02=0xFF;
    if(here==0x89 && (magic00&0x80)==0){//The magic pool
      magic00=0xFF;
      process_output("A strange magic reveals to you the secret of concealed doors.\n");
      reveal_concealed_doors();
    } else
      process_output("The cool water refreshes you.\n");
    return SUCCESS;
  }
  at1a=here;
  if(here==0x3F || here==0x9D){
    drink_beer();
  } else if(!player_has(JUG)){
    process_output("There is no water here.\n");
  } else if(jug->code==JUG_BEER){
    drink_beer();
    empty_jug();
  } else if(jug->code==JUG_WATER){
    process_output("The cool water refreshes you.\n");
    thirst02=0xFF;
    empty_jug();
  } else
    process_output("There is no water here.\n");
  return SUCCESS;
}

//SWIM, see $18B0
int swim(){
  if(!at_water())
    process_output("There is no water here.\n");
  else if(!at_deep_water())
    process_output("The water is too shallow.\n");
  else {
    if(get_player_location_id()==0x89){//The magic pool heals
      process_output("You feel strangely healthy.\n");
      get_player()->strength=0xBE;
    }
    process_output("The water is cool.\n");
  }
  return SUCCESS;
}

//Returns the new player location code if move happened.
//INVALID_MOVE if not.
int move(UCHAR dir, UCHAR dirindex){
  int i;
  if(dir==0)
    return INVALID_MOVE;
  struct location *l = get_player_location();
  struct exit *ematch=NULL;
  for(i=0;i<l->nexits;i++){//$23E3 look for exact match
    struct exit *e = (l->exits)+i;
    if(e->direction==dir){
      ematch=e;
      break;
    } 
  }
  if(ematch==NULL){//Failed to find exact match
    for(i=0;i<l->nexits;i++){//$23FF look for partial match
      struct exit *e = (l->exits)+i;
      if( ( e->direction|dir) == e->direction ){ //eg E will match NE
	ematch=e;
	break;
      } 
    }    
  }
  UCHAR type=get_player_location()->locationtype;
  if(ematch==NULL){
    if( (dir&1) !=0 )//User asked to go down, $2418
      type=0;
    report_invalid_move(ematch,dirindex,type);
    return INVALID_MOVE;
  } else {
    if(ematch->ispassable==IMPASSABLE){
      report_invalid_move(ematch,dirindex,0);
      return INVALID_MOVE;
    } 
    set_player_location_id(ematch->destination);
    return MOVED;
  }
}

void report_invalid_move(struct exit *e, UCHAR dirindex, UCHAR type){
  char w[100];
  char line[1000];
  getword(w, dirindex);
  //These are given in order they appear in code from $247A
  switch(type){ //type is 0-7
    case 1: sprintf(line,"Steep lag heaps block your way.\n"); break;
    case 2: sprintf(line,"The cliff is too steep.\n"); break;
    case 3: sprintf(line,"The mountains are too steep.\n"); break;
    case 4: sprintf(line,"There are bushes in your way.\n"); break;
    case 5: sprintf(line,"Dense undergrowth blocks your way.\n"); break;
    case 6: sprintf(line,"No passage leads %s.\n",w); break;
    case 7: sprintf(line,"The dunes are too steep.\n"); break;
    case 0: sprintf(line,"You can't go %s.\n",w); break;
  }
  process_output(line);
  return;
}

UCHAR get_match(UCHAR *word){
  char w[100];
  UCHAR code;
  for(code=0;code<0xFD;code++){//$FE is *** marking end of $3600 word table
    getword(w,code);
    if(strstr(w,word)==w)//Is seach term match for first part of w?
      return code;
  }
  return NOMATCH;
}

//Copys string, creating memory for to string - remember to free it!
char * create_copy_string(char *from){
  int len=strlen(from);
  char *to=(char *) malloc(len+1);
  while(len>=0){//Copy string and drop to lower case
    to[len]=from[len];
    len--;
  }
  return to;
}

char * sfirstcaps(char *s){
  s[0]=toupper(s[0]);
  return s;
}

//An object's description and properties come from its code, which changes
//when the lamp is lit or the jug filled, see $2580
void set_object_code(struct object *o, UCHAR code){
  char desc[MAXLINE];
  o->code=code;
  printobjectcodedescription(desc,code);
  free(o->description);
  o->description=create_copy_string(desc);
  o->size = getbyte("2600",code);
  o->strength_required = getbyte("262A",code);
  o->durability = getbyte("267E",code);
  o->max_throw_damage = getbyte("26A8",code);
  o->rnd_throw_damage = getbyte("26D2",code);
  o->max_melee_damage = getbyte("26FC",code);
  o->rnd_melee_damage = getbyte("2726",code);
}

void load_object(struct object *o, UCHAR id){
  char desc[MAXLINE];
  UCHAR  code = getbyte("2580",id);

  o->location_id=getbyte("25C0",id);
  o->description=NULL;
  set_object_code(o,code);
}

void load_character(struct character *c, UCHAR id){
  char desc[MAXLINE];
  UCHAR  classcode = getbyte("2968",id);
  UCHAR  class = classcode&0x1F; //$1BEA and $1BA8 mask off bits 5-7

  c->location_id=getbyte("29B4",id);
  c->strength=getbyte("298E",id);
  printcharacterdescription(desc, class);
  c->description=create_copy_string(desc);
  c->classcode=classcode;
  c->class=class;
  c->ishostile=NONHOSTILE;
  if( (classcode&0x80) != 0 ){
    c->ishostile=HOSTILE;
  }
  c->amount_carried=getbyte("29DA",id);//Indexed by character, see $1B9F
  c->max_carried=getbyte("28E0",class);//Indexed by class, see $1BA8
  //Also need stuff in $2880 and 28A0
}

void load_location(struct location *l, UCHAR id){
  long addr=getlocationaddress(id);
  char desc[MAXLINE];
  int i;
  l->id=id;
  //Set up the description
  namelocation(desc,addr);
  l->description=create_copy_string(desc);
  UCHAR b79=ctkv[addr];
  if(b79>=0x80)
    l->isdark=DARK;
  else
    l->isdark=LIGHT;
  l->locationtype=b79&0x07; //Used for describing exits to this place, see $2038
  //Work out number of exits
  UCHAR b7A=ctkv[addr+1];
  UCHAR  nwords=b7A&0x1F;
  l->nexits=b7A>>5;
  l->exits = (struct exit *) malloc(l->nexits*sizeof(struct exit));
  addr+=2+nwords;  //Address for direction/destination bytes
  for(i=0;i<l->nexits;i++){
    addr=load_exit((l->exits)+i,addr);
  }
}

long load_exit(struct exit *e, long addr){
    UCHAR  first = ctkv[addr],second = ctkv[addr+1],third = ctkv[addr+2]; //For two or three bytes
    e->addr=addr;
    if(first <0x80) // Positive byte, two bytes follow
      addr+=2;
    else // Negative byte, three bytes follow
      addr+=3;
    e->direction=first&0x3F; //What do bits 4 and 5 represent?
    e->destination=second;
    e->ispassable=PASSABLE;
    e->exittype=NORMAL;
    e->exittext=NULL;
    //See $21EE for logic of following
    if( (first&0x80)!=0 ) {//Triplet of bytes ie fence, door, grate etc
      if( (third&0x80)==0 ){//Third byte bit 7 not set, not a door or grate
	if((third&0x40)!=0) //Bit six set, see $226F
	  e->exittype=NONDOOR_SEETHRU;
	else
          e->exittype=NONDOOR; //$2255
	e->exittext=get_nondoor_text(first,third);
      } else {//Third byte set so set lockable to type of key/door
	e->exittype=DOOR;
	if( (first&0x40) == 0 ) //See just before $2217
	  e->islocked=LOCKED;
	else
	  e->islocked=UNLOCKED;
	e->exittext=get_door_text(first,third);
      }      
    }
    if( (first&0x40)==0 )//if bit 6 is NOT set    
      e->ispassable=IMPASSABLE;
    e->dirtext=get_dirtext(first);
    return addr;
}

char * get_nondoor_text(UCHAR firstbyte, UCHAR thirdbyte){//$2255 Load text as per $2255
  char text[1000], *ret;
  UCHAR byte=thirdbyte&0x07;
  long addr=strtol("295F",NULL,16)-start;
  int pos=2;
  sprintf(text,"a ");
  if(byte!=0)
    pos=print_word_or_zero(text, pos, addr, byte);

  byte=(thirdbyte>>3)&0x0F;//See $2255
  if( (firstbyte&0x40)!=0 )//If dir byte bit 6 set
    byte=byte|0x20;        //set bit 5  
  addr=strtol("2900",NULL,16)-start;  //$2275
  pos=print_word_or_zero(text, pos, addr, byte);
  addr=strtol("2930",NULL,16)-start;
  pos=print_word_or_zero(text, pos, addr, byte);
  //The "through which" bit is handled in-game as dest structs not all loaded here
  
  text[pos-1]='\0';//Last char will be space, overwrite.
  return create_copy_string(text);
}

char * get_door_text(UCHAR firstbyte, UCHAR thirdbyte){//$2220
  char text[1000], *ret;
  UCHAR byte=thirdbyte&0x07;
  long addr=strtol("295F",NULL,16)-start; //BRASS, BRONZE etc
  int pos=0;
  if(byte!=0)
    pos=print_word_or_zero(text, pos, addr, byte);

  byte=(thirdbyte>>3)&0x0F;//See $2255
  byte=byte|0x10;        //set bit 4  
  addr=strtol("2900",NULL,16)-start; //$2275
  pos=print_word_or_zero(text, pos, addr, byte);
  addr=strtol("2930",NULL,16)-start;
  pos=print_word_or_zero(text, pos, addr, byte);
  //The "through which" bit is handled in-game as dest structs not all loaded here
  
  text[pos-1]='\0';//Last char will be space, overwrite.
  return create_copy_string(text);
}

/* If byte!=0, print word specified by address addr+byte into pos in ss.
   Return value is pos of final space.
   Print nothing if byte==0: pos is returned unchanged.
   See $2275 */
int print_word_or_zero(char *ss, int pos, long addr, UCHAR byte){
  byte=ctkv[addr+byte];
  if(byte!=0){//if zero print nothing, see $1FD0
    addr=getcommandaddress(byte);
    pos+=getwordforaddress(ss+pos,addr);
    ss[pos-1]=' ';
  }
  return pos;
}

char * get_dirtext(UCHAR dirbyte){
  char word[100];
  char *s;
  printdirectiondescription(word,dirbyte);
  s=create_copy_string(word);
  s[0]=toupper(s[0]);
  return s;
}

UCHAR is_dark(UCHAR id){
  struct location *l = get_location(id);
  if(l->isdark==LIGHT)
    return LIGHT;
  struct object *lamp = get_object(LAMP);
  if( (lamp->location_id==CARRIED || lamp->location_id==get_player_location_id()) && lamp_is_lit() )
    return LIGHT;
  
  return DARK;
}

void print_description(UCHAR id){
  if(is_dark(id)==DARK){
    process_output("It is dark here.\n");
  } else {
    print_short_description(id);
    print_long_description(id); //Need to make this conditional
    print_objects(id);
  }
}

void print_objects(UCHAR id){
  int i;
  for(i=0;i<getnumberofobjects();i++){
    char w[1000];
    struct object *obj=get_object(i);
    if(obj->location_id==get_player_location_id()){
      sprintf(w,"There is %s here.\n",obj->description);
      process_output(w);
    }
  }
}

void print_short_description(UCHAR id){
  struct location *l = get_location(id);
  char line[5000];
  sprintf(line,"You are %s.\n",l->description);
  process_output(line);
}

void print_long_description(UCHAR id){
  char text[5000];
  struct location *l = get_location(id);
  int i;
  for(i=0;i<l->nexits;i++){
    struct exit e=(l->exits)[i];
    struct location *dest=get_location(e.destination);
    char line[5000];
    sprintf(line,"%s ",e.dirtext);
    process_output(line);
    if(e.exittype==NORMAL) {
      you_can_see(text,*dest);
      process_output(text);
    }
    else {//door or grate or fence or something else?
      process_output("is ");
      if(e.exittype==NONDOOR || e.exittype==NONDOOR_SEETHRU){//eg fence
	process_output(e.exittext);
	if(e.exittype==NONDOOR_SEETHRU){//If dir byte bit 6 set, $2243
	  sprintf(text," through which ");//15 chars
	  you_can_see(text+15,*dest);
	  process_output(text);	  
	}
      } else {//it's a door or grate
	if(e.islocked==LOCKED)
	  process_output("a locked ");
	else
	  process_output("an open ");
	process_output(e.exittext);
	if(strstr(e.exittext,"grate")!=NULL){
	  sprintf(text," through which ");//15 chars
	  you_can_see(text+15,*dest);
	  process_output(text);
	}
      }
    }
    process_output(".\n");
  }
}

void you_can_see(char *text, struct location dest){
  char *ss;
  char s[20];
  ss=s;
  if(dest.locationtype==0)
    ss=strchr(dest.description,' ')+1; //Skip first word
  else
    incodeexits(ss,dest.locationtype);
  sprintf(text,"you can see %s",ss);
}

//Lists the characters with the player, see $0C37
void print_npc_info(){
  int id;

  if(is_dark(get_player_location_id())==DARK)//$0C4A
    return;
  for(id=getnumberofcharacters()-1;id>0;id--){
    struct character *npc = get_npc(id);
    if(npc->location_id==get_player()->location_id){
      char desc[MAXLINE];
      snprintf(desc,sizeof(desc),"%s",npc->description);
      process_output(sfirstcaps(desc));
      inventory(id); //Prints " is here with ...
    }
  }
}

/* ---------------------------------------------------------------------
 * Characters: what they decide to do each turn and how fights go.
 * Ported from $0C00-$0EDF and $1B6C-$1D9B. Each routine runs twice a
 * turn: first with pass15=0, when it only decides (consuming the same
 * random numbers), then with pass15=$80, when it acts and reports.
 * --------------------------------------------------------------------- */

//Pseudo random byte, see $1C88: a 24-bit shift register in $1D-$1F
//shifted left eight times, feeding in bit 6 XOR bit 3 of $1D
UCHAR rnd(){
  int i;
  for(i=0;i<8;i++){
    UCHAR in=((rnd1d>>6)^(rnd1d>>3))&1;
    UCHAR c1=rnd1f>>7, c2=rnd1e>>7;
    rnd1f=(rnd1f<<1)|in;
    rnd1e=(rnd1e<<1)|c1;
    rnd1d=(rnd1d<<1)|c2;
  }
  return rnd1d;
}

//Prints a sentence, capitalising its first letter
void sentence(const char *fmt, ...){
  char line[MAXLINE];
  va_list ap;
  va_start(ap,fmt);
  vsnprintf(line,sizeof(line),fmt,ap);
  va_end(ap);
  process_output(sfirstcaps(line));
}

//$1C74 prints IS after a character, or ARE after the player
const char * is_are(UCHAR id){
  return id==0 ? "are" : "is";
}

//Where an object is, counting the holdall's contents as wherever the holdall is
UCHAR carrier_location(UCHAR objid){
  UCHAR loc=get_object(objid)->location_id;
  if(loc==0xFF)
    loc=get_object(0)->location_id;
  return loc;
}

//Moves an object to dest, see $1B6C. Returns 1 if the destination can't take it.
//Unless bit 7 of flag is set, only checks whether it could.
int transfer(UCHAR objid, UCHAR dest, UCHAR flag){
  struct object *o=get_object(objid);
  UCHAR from=o->location_id;
  if(dest<CARRIED){//Dropped somewhere
    if( (flag&0x80)==0 )
      return 0;
  } else if(dest==0xFF){//Into the holdall, $1B88: holds less than $64
    int w=holdallweight+o->size;
    if(w>0xFF || w>=0x64)
      return 1;
    holdallweight=w;
  } else {//Carried by a character, $1B9F
    struct character *c=get_npc(dest-CARRIED);
    int w=c->amount_carried+o->size;
    if(w>0xFF || w>=c->max_carried)
      return 1;
    if( (flag&0x80)==0 )
      return 0;
    c->amount_carried=w;
  }
  o->location_id=dest;//$1BC4
  if(from==0xFF)
    holdallweight-=o->size;
  else if(from>=CARRIED)
    get_npc(from-CARRIED)->amount_carried-=o->size;
  return 0;
}

//$1D20: a weapon is too heavy if it needs at least its carrier's strength
int weapon_too_weak(UCHAR objid){
  struct character *c=get_npc(carrier_location(objid)-CARRIED);
  return get_object(objid)->strength_required >= c->strength;
}

//Hits victim with a carried weapon, see $1CCA. Returns 1 if the carrier is too weak.
int melee_attack(UCHAR objid, UCHAR victim, UCHAR flag){
  struct object *o=get_object(objid);
  UCHAR carrier=carrier_location(objid)-CARRIED;
  if(weapon_too_weak(objid))
    return 1;
  if( (flag&0x80)==0 )
    return 0;
  if(at1a==get_player_location_id())
    sentence("%s %s hit with %s.\n",get_npc(victim)->description,is_are(victim),o->description);
  status[carrier]=0x80;//$1CBE: remember who attacked
  inflict(victim,o->max_melee_damage,o->rnd_melee_damage);
  if(rnd()<0xD2){//$1CFB: the weapon is usually kept
    if(o->code!=STAFF)
      return 0;
    if(--staff27>=0)
      return 0;
  }
  if(at1a==get_player_location_id())//$1D0B
    sentence("%s drops.\n",o->description);
  weapon_lands(objid,flag);
  return 0;
}

//Throws a carried weapon at victim, see $1D40. Returns 1 if the carrier is too weak.
int throw_attack(UCHAR objid, UCHAR victim, UCHAR flag){
  struct object *o=get_object(objid);
  UCHAR carrier=carrier_location(objid)-CARRIED;
  if(weapon_too_weak(objid))
    return 1;
  if( (flag&0x80)==0 )
    return 0;
  if(at1a==get_player_location_id())
    sentence("%s is thrown at %s.\n",o->description,get_npc(victim)->description);
  status[carrier]=0x80;
  inflict(victim,o->max_throw_damage,o->rnd_throw_damage);
  weapon_lands(objid,flag);
  return 0;
}

//A thrown or dropped weapon breaks or falls where the fight is, see $1D71
void weapon_lands(UCHAR objid, UCHAR flag){
  struct object *o=get_object(objid);
  UCHAR durability=getbyte("267E",o->code);
  if( (rnd()&durability)==durability ){
    if(at1a==get_player_location_id())
      sentence("%s breaks.\n",o->description);
    transfer(objid,BROKEN,flag);
  } else
    transfer(objid,at1a,flag);
}

//Damage is max less a random amount masked by rndmask, see $1C9E
void inflict(UCHAR victim, UCHAR max, UCHAR rndmask){
  damage(victim,(UCHAR)(max-(rnd()&rndmask)));
}

//Takes amount off victim's health and reports how they are, see $1C02
void damage(UCHAR victim, UCHAR amount){
  struct character *c=get_npc(victim);
  int here = at1a==get_player_location_id();
  if(amount>=c->strength){//$1C31: dead
    if(here)
      sentence("%s %s dead.\n",c->description,is_are(victim));
    c->strength=0;
    if(victim==0){//$1C4D: the game is over
      print_score_and_strength();
      process_output("Bye bye.\n");
      exit(0);
    }
    c->location_id=BROKEN;
    int x;
    for(x=getnumberofobjects()-1;x>=0;x--)//$1C61: drops everything it had
      if(get_object(x)->location_id==CARRIED+victim)
        transfer(x,at1a,0x80);
    return;
  }
  c->strength-=amount;
  if(c->strength>=0x50 || !here)
    return;
  sentence("%s %s %s.\n",c->description,is_are(victim),c->strength>=0x28?"weak":"dying");
}

//$12EE. The score is the value of the treasure in the cabin or carried, $130A.
void print_score_and_strength(){
  int x;
  unsigned int score=0;
  for(x=getnumberofobjects()-1;x>=0;x--){
    UCHAR loc=get_object(x)->location_id;
    if(loc==0xFF)
      loc=get_object(0)->location_id;
    if(loc==0 || loc==CARRIED)
      score+=getbyte("2654",get_object(x)->code);
  }
  sentence("Your score is %u.\n",score);
  sentence("Your strength is %u.\n",get_player()->strength);
}

//$3B8D: does the player carry the object, or have it in a carried holdall?
int player_has(UCHAR objid){
  UCHAR loc=get_object(objid)->location_id;
  return loc==CARRIED || (loc==0xFF && get_object(0)->location_id==CARRIED);
}

int lamp_is_lit(){
  return get_object(LAMP)->code==0;
}

//$245A: puts the player somewhere and describes it
void move_player(UCHAR id){
  set_player_location_id(id);
  print_description(id);
}

//Re-reads a location after its data in ctkv has been changed, as some
//events do to open passages or move bridges
void reload_location(UCHAR id){
  free_location(get_location(id));
  load_location(get_location(id),id);
}

//A word as $14B8 prints it, in one of several buffers so a few can be
//used in one sentence
char * word_of(UCHAR code){
  static char words[4][MAXWORD];
  static int next=0;
  char *w=words[next];
  next=(next+1)%4;
  getanyword(w,code);
  return w;
}

//$15D2: finds the object the carrier ($C8 for the player) has that best
//matches the words, asking "<prompt> what?" once if there's none.
//Returns the number of words matched, 0 if still none.
int ask_carried(UCHAR carrier, UCHAR prompt, UCHAR *matches, int *nmatches, UCHAR *id){
  if(match_carried_object(carrier,matches,*nmatches,id))
    return 1;
  sentence("%s what",word_of(prompt));
  process_output("?");
  if(read_command(matches,nmatches)!=SUCCESS)
    *nmatches=0;
  return match_carried_object(carrier,matches,*nmatches,id);
}

//Counts how many of the given words match word code w, see $152F
int count_word(UCHAR w, UCHAR *matches, int nmatches){
  int i,n=0;
  if(w==0)
    return 0;
  for(i=0;i<nmatches;i++)
    if(matches[i]==w)
      n++;
  return n;
}

//Finds the character at loc best matching the words, see $1D9C.
//Returns the number of matching words, 0 if none, with the character in *id.
int match_character(UCHAR loc, UCHAR *matches, int nmatches, UCHAR *id){
  int x,best=0;
  for(x=getnumberofcharacters()-1;x>=0;x--){
    struct character *c=get_npc(x);
    if(c->location_id!=loc)
      continue;
    int n=count_word(getbyte("2820",c->class),matches,nmatches)
         +count_word(getbyte("2840",c->class),matches,nmatches)
         +count_word(getbyte("2860",c->class),matches,nmatches);
    if(n>best){
      best=n;
      *id=x;
    }
  }
  return best;
}

//Finds the object at loc best matching the words, see $14EF
int match_object_words(UCHAR loc, UCHAR *matches, int nmatches, UCHAR *id){
  int x,best=0;
  for(x=getnumberofobjects()-1;x>=0;x--){
    struct object *o=get_object(x);
    if(o->location_id!=loc)
      continue;
    int n=count_word(getbyte("2750",o->code),matches,nmatches)
         +count_word(getbyte("277A",o->code),matches,nmatches)
         +count_word(getbyte("27A4",o->code),matches,nmatches)
         +count_word(getbyte("27CE",o->code),matches,nmatches);
    if(n>best){
      best=n;
      *id=x;
    }
  }
  return best;
}

//An object at carrier, or in the holdall if that is there too, see $15A7
int match_carried_object(UCHAR carrier, UCHAR *matches, int nmatches, UCHAR *id){
  UCHAR inholdall;
  int n=match_object_words(carrier,matches,nmatches,id);
  if(get_object(0)->location_id==carrier){
    int m=match_object_words(0xFF,matches,nmatches,&inholdall);
    if(m>n){
      *id=inholdall;
      n=m;
    }
  }
  return n;
}

//Asks a question such as "Hit who" and reads the answer, see $229F
void ask_again(UCHAR word, UCHAR *matches, int *nmatches){
  sentence("%s who",word_of(word));
  process_output("?");
  if(read_command(matches,nmatches)!=SUCCESS)
    *nmatches=0;
}

//$1DFD: finds the character here and the carried object the words name.
//Asks "<objprompt> what?" once if there's no object, then "<whoprompt> who?"
//once if there's no character. Returns 0, having said why, if either is missing.
int find_who_and_what(UCHAR objprompt, UCHAR whoprompt, UCHAR *matches, int *nmatches,
                      UCHAR *who, UCHAR *what){
  UCHAR ploc=get_player_location_id();
  int found=match_character(ploc,matches,*nmatches,who);
  if(ask_carried(CARRIED,objprompt,matches,nmatches,what)==0){
    process_output("You haven't got that.\n");
    return 0;
  }
  if(found==0)//$1E17: perhaps named in the answer
    found=match_character(ploc,matches,*nmatches,who);
  if(found==0){
    ask_again(whoprompt,matches,nmatches);
    if(match_character(ploc,matches,*nmatches,who)==0){
      process_output("I can't see any here.\n");
      return 0;
    }
  }
  at1a=ploc;
  return 1;
}

//HIT, CUT or THROW, see $1DDA
void player_attack(UCHAR verb, UCHAR *matches, int nmatches){
  UCHAR victim=0, objid=0;
  //"With what?" or "Throw what?", then "Hit who?", "Cut who?" or "At who?"
  if(!find_who_and_what(verb==0x10 ? verb : 0xB3, verb==0x10 ? 0x49 : verb,
                        matches,&nmatches,&victim,&objid))
    return;
  if(weapon_too_weak(objid)){//$1E62
    process_output("You are too weak.\n");
    return;
  }
  if(verb==0x10)
    throw_attack(objid,victim,0xFF);//$1D45
  else
    melee_attack(objid,victim,0xFF);//$1CCF
}

//GIVE, see $1E43: "Give what?", then "To who?"
int give(UCHAR verb, UCHAR *matches, int nmatches){
  UCHAR who=0, what=0;
  if(!find_who_and_what(verb,0x43,matches,&nmatches,&who,&what))
    return EMPTY;
  if(transfer(what,CARRIED+who,0xFF))//$1599
    sentence("%s can't carry any more.\n",get_npc(who)->description);
  else
    process_output("OK.\n");
  return SUCCESS;
}

//$133D: the player is handed an object. Returns 1 if they can't carry it.
int given(UCHAR id){
  sentence("You are given %s.\n",get_object(id)->description);
  return transfer(id,CARRIED,CARRIED);
}

//ASK, see $1E74: "Ask who?", then "For what?"
int ask(UCHAR *matches, int nmatches){
  UCHAR who=0, what=0;
  UCHAR ploc=get_player_location_id();
  if(match_character(ploc,matches,nmatches,&who)==0){
    process_output("Ask who?");
    if(read_command(matches,&nmatches)!=SUCCESS)
      nmatches=0;
    if(match_character(ploc,matches,nmatches,&who)==0){
      process_output("I can't see any here.\n");
      return EMPTY;
    }
  }
  if(ask_carried(CARRIED+who,0xB8,matches,&nmatches,&what)==0){//$15D4
    sentence("%s hasn't got that.\n",get_npc(who)->description);
    return EMPTY;
  }
  if(who==0x10 || (get_npc(who)->classcode&0x80)){
    process_output("Your request is refused.\n");
    return SUCCESS;
  }
  if(given(what))
    process_output("You can't carry any more.\n");
  return SUCCESS;
}

UCHAR wave03=0x80; //$03: which way the rod last set the chasm crossing

//WAVE, see $1391. Waving the rod at either end of the chasm ($19, $23)
//changes where their first exits lead, $97 or $33, see the table at $1380.
int wave(UCHAR *matches, int nmatches){
  UCHAR id;
  UCHAR here=get_player_location_id();
  if(match_carried_object(CARRIED,matches,nmatches,&id)==0){
    if(nmatches>=2)//$15F6
      process_output("You haven't got that.\n");
    else
      process_output("Nothing happens.\n");
    return SUCCESS;
  }
  if(id!=0x07 || (here!=0x19 && here!=0x23)){
    process_output("Nothing happens.\n");
    return SUCCESS;
  }
  wave03 = wave03 ? 0 : 6;
  UCHAR dest=getbyte("1385",wave03);
  UCHAR ends[2]={0x19,0x23};
  int i;
  for(i=0;i<2;i++){//$13F3
    ctkv[get_location(ends[i])->exits[0].addr+1]=dest;
    reload_location(ends[i]);
  }
  print_description(here);//$2517
  return SUCCESS;
}

//HELP, see $1356: lists the commands $2E down to $0E
void help(){
  int code,col=0;
  process_output("I understand the following,\n");
  for(code=FIRSTFREE+9;code>=FIRSTCOMBAT;code--){
    char *w=word_of(code);
    int len=strlen(w);
    if(col+len+1>39){
      process_output("\n");
      col=0;
    }
    process_output(w);
    process_output(" ");
    col+=len+1;
  }
  process_output("\nand any direction.\n");
}

//Finds the healthiest character at at1a on the other side, see $0D8D
UCHAR find_enemy(){
  int x;
  result10=0;
  for(x=getnumberofcharacters()-1;x>=0;x--){
    struct character *c=get_npc(x);
    if(c->location_id!=at1a)
      continue;
    if( ((c->classcode^align1b)&0x80)==0 )//Same side
      continue;
    if(status[0]&0x40)//Player getting away: nobody fights
      continue;
    if(c->strength<result10)
      continue;
    result10=c->strength;
    target17=x;
  }
  return result10;
}

//Can the character go to dest? Each set bit of 4-7 of prop18 allows a
//region of four location ranges in the tables at $1263-$1282, see $0DFF.
//Returns 0 and, when acting, records the move ($0E3C).
int npc_try_dest(UCHAR dest){
  int y,i;
  UCHAR p=prop18;
  for(y=3;y>=0;y--,p<<=1){
    if( (p&0x80)==0 )
      continue;
    for(i=0;i<4;i++){
      UCHAR lo=getbyte("1263",4*i+y), hi=getbyte("1273",4*i+y);
      if(dest>=lo && dest<hi){
        if(pass15&0x80){
          nextloc[npc16]=dest;
          status[npc16]|= (prev13&0x80) ? 0x40 : 0x20;
        }
        return 0;
      }
    }
  }
  return 1;
}

//Tries exit k (1-based) of location l, see $0DE2. Returns 0 if it's taken.
int npc_try_exit(struct location *l, int k){
  struct exit *e=l->exits+k-1;
  result10=k;
  if(e->ispassable==IMPASSABLE)//Bit 6 of the direction byte
    return 1;
  return npc_try_dest(e->destination);
}

//Picks a random exit, or failing that the last usable one, see $0DB9
void npc_random_move(){
  struct location *l;
  int k;
  result10=0;
  if(at1a>=NLOCATIONS)
    return;
  l=get_location(at1a);
  if(l->nexits==0)
    return;
  do
    k=rnd()&0x07;
  while(k>=l->nexits);
  if(npc_try_exit(l,k+1)==0){
    result10=0xFF;
    return;
  }
  for(k=l->nexits;k>0;k--)
    if(npc_try_exit(l,k)==0){
      result10=0xFF;
      return;
    }
  result10=0;
}

//Wanders off with a chance set by bits 0-3 of prop18, see $0D79
void npc_maybe_wander(){
  result10=rnd()&0x0F;
  if( (prop18&0x0F)>=result10 )
    npc_random_move();
  else
    result10=0;
}

//Follows the player's trail if on it, otherwise may wander, see $0D3D
void npc_follow(){
  UCHAR dest;
  result10=0xFF;
  if(at1a==loc1c)//Where the player was two turns ago: go where they were last turn
    dest=loc14;
  else if(at1a!=loc14){
    npc_maybe_wander();
    return;
  } else//Where the player was last turn: go where they are now
    dest=get_player_location_id();
  if(dest==at1a){
    result10=0;
    return;
  }
  npc_try_dest(dest);//result10 stays $FF even if it can't go there
}

//Picks up the first weapon lying here that it can carry, see $0E99
void npc_pickup(){
  int x;
  result10=0;
  for(x=FIRSTWEAPON;x<ENDWEAPON;x++){
    if(get_object(x)->location_id!=at1a)
      continue;
    if(transfer(x,CARRIED+npc16,pass15))
      continue;
    if( (pass15&0x80) && at1a==get_player_location_id() )
      sentence("%s takes %s.\n",get_npc(npc16)->description,get_object(x)->description);
    result10=0xFF;
    return;
  }
}

//Attacks the enemy found by find_enemy() with the first usable weapon,
//usually hitting, sometimes throwing. With no weapon, picks one up. See $0E5B.
void npc_attack(){
  int x;
  result10=0;
  for(x=FIRSTWEAPON;x<ENDWEAPON;x++){
    if( (UCHAR)(carrier_location(x)-CARRIED)!=npc16 )
      continue;
    int failed = rnd()>=0xC0 ? throw_attack(x,target17,pass15)
                             : melee_attack(x,target17,pass15);
    if(!failed){
      result10=0xFF;
      return;
    }
  }
  npc_pickup();
}

//$0CDE: a living character takes its turn
void npc_act(UCHAR x){
  if(get_npc(x)->strength==0)
    return;
  npc_turn(x);
}

//Decides, and when acting carries out, what character x does, see $0CEE
void npc_turn(UCHAR x){
  struct character *c=get_npc(x);
  npc16=x;
  at1a=c->location_id;
  prev13=status[x];
  if(pass15&0x80)
    status[x]&=0x7F;
  align1b=c->classcode;
  prop18=getbyte("2880",c->class);
  prop19=getbyte("28A0",c->class);
  if( (prop18|prop19)==0 ){//Never moves: fights or picks up weapons
    if(find_enemy())
      npc_attack();
    else
      npc_pickup();
    return;
  }
  if(find_enemy()==0){//$0D6E: nobody to fight
    npc_pickup();
    if(result10)
      return;
    if(prop19)
      npc_follow();
    else
      npc_maybe_wander();
    return;
  }
  if(prop19){//A follower
    if( (prev13&0x80)==0 ){//$0D55: didn't attack last turn
      npc_attack();
      if(result10)
        return;
      npc_follow();
      if(result10)
        return;
      npc_random_move();
      return;
    }
    npc_follow();
    if(result10)
      return;
  }
  npc_attack();//$0D36
  if(result10==0){
    npc_random_move();
    return;
  }
  npc_follow();
}

struct character * get_player(){
  return characters;
}

//id=0 is player
struct character * get_npc(UCHAR id){
  return characters+id;
}

struct location * get_location(UCHAR i){
  return locations+i;
}

struct object * get_object(UCHAR i){
  return objects+i;
}

struct location * get_player_location(){
  return locations+get_player_location_id();
}

UCHAR get_player_location_id(){
  return characters->location_id;
}

void set_player_location_id(UCHAR id){
  characters->location_id=id;
}

void cleanup(){
  int i;
  for(i=0;i<NLOCATIONS;i++)
    free_location(locations+i);
  for(i=0;i<getnumberofobjects();i++)
    free_object(objects+i);
  for(i=0;i<getnumberofcharacters();i++)
    free_character(characters+i);
  free(locations);
  free(objects);
  free(characters);
}

void free_location(struct location *l){
  int i;
  free(l->description);
  for(i=0;i<l->nexits;i++){
    free((l->exits)[i].dirtext);
    if((l->exits)[i].exittext != NULL )
      free((l->exits)[i].exittext);
  }
  free(l->exits);
}

void free_object(struct object *o){
  free(o->description);
}

void free_character(struct character *c){
  free(c->description);
}