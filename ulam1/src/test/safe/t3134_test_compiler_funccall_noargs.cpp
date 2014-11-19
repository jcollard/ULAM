#include "TestCase_EndToEndCompiler.h"

namespace MFM {

  BEGINTESTCASECOMPILER(t3134_test_compiler_funccall_noargs)
  {
    std::string GetAnswerKey()
    {
      //updated to even word boundary
      //Ue_A { Int(32) a(0);  Int(32) test() {  ( )foo return } }\nExit status: 1
      //return std::string("Ue_A { Bool(7) b(false);  Int(32) a(0);  Int(32) test() {  ( )foo return } }\nExit status: 1");
      //updated to print a in gencode
      return std::string("Ue_A { Bool(7) b(false);  System s();  Int(32) a(1);  Int(32) test() {  a ( )foo = s ( a )print . a return } }\nExit status: 1");
    }
    
    std::string PresetTest(FileManagerString * fms)
    {
      //bool rtn1 = fms->add("A.ulam","element A {\nBool(7) b;\nInt a;\nInt foo() {\n return 1;\n }\nInt test() {\nreturn foo();\n}\n}\n");
      bool rtn1 = fms->add("A.ulam","use System; element A {\nSystem s;\nBool(7) b;\nInt a;\nInt foo() {\n return 1;\n }\nInt test() {\na = foo();\ns.print(a);\nreturn a;\n}\n}\n");
            
      bool rtn3 = fms->add("System.ulam", "ulam 1;\nquark System {Void print(Unsigned arg) native;\nVoid print(Int arg) native;\nVoid print(Int(4) arg) native;\nVoid print(Int(3) arg) native;\nVoid assert(Bool b) native;\n}\n");      

      if(rtn1 && rtn3)
	return std::string("A.ulam");
      
      return std::string("");
    }
  }
  
  ENDTESTCASECOMPILER(t3134_test_compiler_funccall_noargs)
  
} //end MFM


