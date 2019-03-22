// Routines to log nd show debug messages on html page

#define DEBUG_MAX_RECORDS 20
class DebugHTML  
{
  private:
  String debugStr[DEBUG_MAX_RECORDS];
  int curRecord = 0; // initial value  

  public:
  
  void init(){
    curRecord = 0;
    for(int i=0; i<DEBUG_MAX_RECORDS; i++){
      debugStr[i]="";
    }
  } 

  void addLine(String strLine){
    String timecnt = "0000000"+String(millis());
    debugStr[curRecord] = timecnt.substring(timecnt.length()-8) + ":" + strLine;
    curRecord++;
    if(curRecord>=DEBUG_MAX_RECORDS){curRecord=0;}
  } 

  String getHTML(){
    String out = "";
    int tmp = curRecord;
    if(curRecord==0){
      // show lines from 0 to MAX
      for(int i=0; i<DEBUG_MAX_RECORDS; i++){
        out+= debugStr[i] + "<BR>";
      }
    }else{
      // show lines from middle til end and then from beginning til middle
      for(int i=tmp; i<DEBUG_MAX_RECORDS; i++){
        out+= debugStr[i] + "<BR>";
      }
      for(int i=0; i<tmp; i++){
        out+= debugStr[i] + "<BR>";
      }
    }
    return out;
  }
};
