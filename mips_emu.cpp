/*
 * ════════════════════════════════════════════════════════════════════
 *  mips_emu.cpp  —  5-Stage Pipelined MIPS Emulator  (CLI)
 * ════════════════════════════════════════════════════════════════════
 *
 *  Build:  g++ -std=c++17 -O2 -o mips_emu mips_emu.cpp
 *
 *  Usage:  mips_emu [options] <file.s | file.asm | file.bin>
 *
 *  Options:
 *    --log <path>    Write final register file to <path>
 *                    (default: registers.log)
 *    --stderr        Send register dump to stderr instead of file
 *    --verbose       Print pipeline diagram every cycle
 *    --max <n>       Stop after n cycles (default: 2,000,000)
 *
 *  Input formats:
 *    .s / .asm       MIPS assembly source (assembled internally)
 *    .bin            Raw big-endian 32-bit MIPS machine-code words
 *    .hex            One hex word per line (0x... or plain hex)
 *
 * ── Supported ISA ──────────────────────────────────────────────────
 *  R-type : add addu sub subu and or xor nor slt sltu
 *           sll srl sra sllv srlv srav
 *           jr jalr mult multu div divu mfhi mflo mthi mtlo syscall
 *  I-type : addi addiu andi ori xori lui slti sltiu
 *           lw lh lb lhu lbu sw sh sb
 *           beq bne blez bgtz  REGIMM: bltz bgez bltzal bgezal
 *  J-type : j  jal
 *  Pseudo : nop move not neg mul
 *           li  la  (expand to 1 or 2 words automatically)
 *           blt bgt ble bge bltu bgtu  (each = 2 words)
 *  Data   : .word .half .byte .space .ascii .asciiz .align
 * ════════════════════════════════════════════════════════════════════
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <functional>
#include <cassert>

// ═══════════════════════════════════════════════════════════════════
//  SECTION 1 — Memory layout & field helpers
// ═══════════════════════════════════════════════════════════════════
static constexpr uint32_t MEM_SIZE  = 1u << 22;          // 4 MB physical
static constexpr uint32_t MEM_MASK  = MEM_SIZE - 1;
static constexpr uint32_t TEXT_BASE = 0x00400000u;        // .text start
static constexpr uint32_t DATA_BASE = 0x10010000u;        // .data start
static constexpr uint32_t STACK_TOP = 0x7FFFFFFCu;

inline uint32_t phys(uint32_t va){ return va & MEM_MASK; }

inline uint8_t  iOP  (uint32_t i){ return (i>>26)&0x3F; }
inline uint8_t  iRS  (uint32_t i){ return (i>>21)&0x1F; }
inline uint8_t  iRT  (uint32_t i){ return (i>>16)&0x1F; }
inline uint8_t  iRD  (uint32_t i){ return (i>>11)&0x1F; }
inline uint8_t  iSH  (uint32_t i){ return (i>> 6)&0x1F; }
inline uint8_t  iFN  (uint32_t i){ return  i     &0x3F; }
inline int32_t  iSIMM(uint32_t i){ return (int16_t)(i&0xFFFF); }
inline uint32_t iUIMM(uint32_t i){ return i & 0xFFFF; }
inline uint32_t iJTGT(uint32_t i){ return i & 0x03FFFFFFu; }

// ═══════════════════════════════════════════════════════════════════
//  SECTION 2 — Register helpers
// ═══════════════════════════════════════════════════════════════════
static const char* RNAME[32] = {
    "$zero","$at","$v0","$v1","$a0","$a1","$a2","$a3",
    "$t0","$t1","$t2","$t3","$t4","$t5","$t6","$t7",
    "$s0","$s1","$s2","$s3","$s4","$s5","$s6","$s7",
    "$t8","$t9","$k0","$k1","$gp","$sp","$fp","$ra"
};

static int parseReg(const std::string& s){
    if(s.empty()) throw std::runtime_error("empty register token");
    std::string r = (s[0]=='$') ? s.substr(1) : s;
    if(!r.empty() && (std::isdigit((unsigned char)r[0]))){
        int n = std::stoi(r);
        if(n<0||n>31) throw std::runtime_error("register out of range: "+s);
        return n;
    }
    static const std::unordered_map<std::string,int> M={
        {"zero",0},{"at",1},{"v0",2},{"v1",3},
        {"a0",4},{"a1",5},{"a2",6},{"a3",7},
        {"t0",8},{"t1",9},{"t2",10},{"t3",11},
        {"t4",12},{"t5",13},{"t6",14},{"t7",15},
        {"s0",16},{"s1",17},{"s2",18},{"s3",19},
        {"s4",20},{"s5",21},{"s6",22},{"s7",23},
        {"t8",24},{"t9",25},{"k0",26},{"k1",27},
        {"gp",28},{"sp",29},{"fp",30},{"ra",31}
    };
    auto it=M.find(r);
    if(it==M.end()) throw std::runtime_error("unknown register: "+s);
    return it->second;
}

// ═══════════════════════════════════════════════════════════════════
//  SECTION 3 — Immediate / number helpers
// ═══════════════════════════════════════════════════════════════════
static bool isNumStr(const std::string& s){
    if(s.empty()) return false;
    size_t i=(s[0]=='-'||s[0]=='+')?1:0;
    if(i>=s.size()) return false;
    if(s.size()>i+1 && s[i]=='0' && (s[i+1]=='x'||s[i+1]=='X')) return true;
    return std::all_of(s.begin()+i, s.end(), [](char c){ return std::isdigit((unsigned char)c); });
}

static int32_t parseNum(const std::string& s){
    if(s.size()>1 && s[0]=='0' && (s[1]=='x'||s[1]=='X'))
        return (int32_t)std::stoul(s,nullptr,16);
    if(s.size()>2 && s[0]=='-' && s[1]=='0' && (s[2]=='x'||s[2]=='X'))
        return -(int32_t)std::stoul(s.substr(1),nullptr,16);
    return std::stoi(s);
}

static std::string parseStringLiteral(const std::string& s){
    // Expect s to start with '"' and end with '"'
    std::string result;
    size_t i=0;
    if(i<s.size()&&s[i]=='"') i++;
    while(i<s.size()&&s[i]!='"'){
        if(s[i]=='\\'&&i+1<s.size()){
            ++i;
            switch(s[i]){
                case 'n': result+='\n'; break;
                case 't': result+='\t'; break;
                case 'r': result+='\r'; break;
                case '0': result+='\0'; break;
                case '\\': result+='\\'; break;
                case '"': result+='"'; break;
                default: result+=s[i]; break;
            }
        } else {
            result+=s[i];
        }
        ++i;
    }
    return result;
}

// ═══════════════════════════════════════════════════════════════════
//  SECTION 4 — Assembler
// ═══════════════════════════════════════════════════════════════════
using SymTab = std::unordered_map<std::string,uint32_t>;

struct AssembledProgram {
    std::vector<uint32_t> textWords;   // encoded machine code
    std::vector<uint8_t>  dataBytes;   // .data segment
    uint32_t textBase = TEXT_BASE;
    uint32_t dataBase = DATA_BASE;
};

// ── Tokeniser ────────────────────────────────────────────────────────
struct ParsedLine {
    std::string              label;
    std::string              mnem;       // instruction or "" (directive or empty)
    std::string              directive;  // e.g. ".word", "" if not a directive
    std::vector<std::string> ops;
    bool isEmpty = false;
};

static std::string trim(const std::string& s){
    size_t a=s.find_first_not_of(" \t\r\n");
    if(a==std::string::npos) return "";
    size_t b=s.find_last_not_of(" \t\r\n");
    return s.substr(a,b-a+1);
}
static std::string toLower(std::string s){
    std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return std::tolower(c);});
    return s;
}

static ParsedLine parseLine(const std::string& rawLine){
    ParsedLine out;
    // Strip comment (# or ;)
    std::string line = rawLine;
    for(size_t i=0;i<line.size();i++){
        if(line[i]=='#'||line[i]==';'){ line=line.substr(0,i); break; }
    }
    line = trim(line);
    if(line.empty()){ out.isEmpty=true; return out; }

    // Extract label  (anything before the first ':' that has no spaces)
    size_t colon = line.find(':');
    if(colon!=std::string::npos){
        std::string lbl=trim(line.substr(0,colon));
        // Only treat as label if no whitespace in it
        bool ok=true;
        for(char c:lbl) if(std::isspace((unsigned char)c)){ok=false;break;}
        if(ok && !lbl.empty()){
            out.label = lbl;
            line = trim(line.substr(colon+1));
            if(line.empty()) return out;
        }
    }

    // Extract mnemonic/directive  (first whitespace-delimited token)
    size_t sp = line.find_first_of(" \t");
    std::string token = (sp==std::string::npos) ? line : line.substr(0,sp);
    std::string rest  = (sp==std::string::npos) ? "" : trim(line.substr(sp));

    if(token[0]=='.'){
        out.directive = toLower(token);
    } else {
        out.mnem = toLower(token);
    }

    // Parse operands
    if(!rest.empty()){
        if(out.directive==".ascii"||out.directive==".asciiz"){
            // Keep the whole rest as one operand (may contain commas in strings)
            out.ops.push_back(rest);
        } else {
            // Split by ',' but respect parentheses
            std::string cur;
            int depth=0;
            for(char c:rest){
                if(c=='(') depth++;
                else if(c==')') depth--;
                if(c==','&&depth==0){
                    std::string t=trim(cur);
                    if(!t.empty()) out.ops.push_back(t);
                    cur.clear();
                } else {
                    cur+=c;
                }
            }
            std::string t=trim(cur);
            if(!t.empty()) out.ops.push_back(t);
        }
    }
    return out;
}

// ── Memory operand splitter  "offset(base)" ───────────────────────
static bool splitMem(const std::string& s, std::string& outOff, std::string& outBase){
    size_t lp=s.find('(');
    if(lp==std::string::npos) return false;
    size_t rp=s.rfind(')');
    if(rp==std::string::npos||rp<=lp) return false;
    outOff  = trim(s.substr(0,lp));
    outBase = trim(s.substr(lp+1,rp-lp-1));
    return true;
}

// ── Resolve immediate (number literal or label) ────────────────────
static int32_t resolveImm(const std::string& s, uint32_t pc, bool isBranch,
                           const SymTab& sym){
    if(isNumStr(s)) return parseNum(s);
    auto it=sym.find(s);
    if(it==sym.end()) throw std::runtime_error("undefined symbol: "+s);
    if(isBranch) return (int32_t)(it->second - (pc+4)) / 4;
    return (int32_t)it->second;
}
static uint32_t resolveJump(const std::string& s, const SymTab& sym){
    if(isNumStr(s)) return (uint32_t)parseNum(s) >> 2;
    auto it=sym.find(s);
    if(it==sym.end()) throw std::runtime_error("undefined symbol: "+s);
    return it->second >> 2;
}

// ── Instruction word encoders ─────────────────────────────────────
static inline uint32_t encR(int rs,int rt,int rd,int sh,int fn){
    return ((rs&31)<<21)|((rt&31)<<16)|((rd&31)<<11)|((sh&31)<<6)|(fn&63);
}
static inline uint32_t encI(int op,int rs,int rt,int16_t imm){
    return ((op&63)<<26)|((rs&31)<<21)|((rt&31)<<16)|(uint16_t)imm;
}
static inline uint32_t encJ(int op,uint32_t tgt){
    return ((op&63)<<26)|(tgt&0x3FFFFFFu);
}

// ── How many words does this mnemonic+ops produce? (for pass 1) ───
static int instrWordCount(const std::string& m, const std::vector<std::string>& ops){
    if(m=="li"){
        if(ops.size()>=2&&isNumStr(ops[1])){
            int32_t v=parseNum(ops[1]);
            return (v>=-32768&&v<=65535) ? 1 : 2;
        }
        return 2; // label — assume large
    }
    if(m=="la")  return 2;
    if(m=="blt"||m=="bgt"||m=="ble"||m=="bge"||
       m=="bltu"||m=="bgtu"||m=="mul") return 2;
    return 1;
}

// ── Encode one instruction → 1 or 2 words ─────────────────────────
static std::vector<uint32_t> encodeInstr(
        const std::string& m,
        const std::vector<std::string>& ops,
        uint32_t pc,
        const SymTab& sym)
{
    // Helpers
    auto reg = [&](size_t i){ return parseReg(ops[i]); };
    auto immB= [&](size_t i){ return resolveImm(ops[i],pc,true,sym); };
    auto immI= [&](size_t i){ return resolveImm(ops[i],pc,false,sym); };
    auto jt  = [&](size_t i){ return resolveJump(ops[i],sym); };

    // Memory operand helpers
    auto mOff=[&](size_t i)->int16_t{
        std::string off,base;
        if(splitMem(ops[i],off,base)) return (int16_t)parseNum(off.empty()?"0":off);
        return 0;
    };
    auto mBase=[&](size_t i)->int{
        std::string off,base;
        if(splitMem(ops[i],off,base)) return parseReg(base);
        if(ops.size()>i+1) return parseReg(ops[i+1]);
        return 0;
    };

    // ── R-type arithmetic ──
    if(m=="add" )return{encR(reg(1),reg(2),reg(0),0,0x20)};
    if(m=="addu")return{encR(reg(1),reg(2),reg(0),0,0x21)};
    if(m=="sub" )return{encR(reg(1),reg(2),reg(0),0,0x22)};
    if(m=="subu")return{encR(reg(1),reg(2),reg(0),0,0x23)};
    if(m=="and" )return{encR(reg(1),reg(2),reg(0),0,0x24)};
    if(m=="or"  )return{encR(reg(1),reg(2),reg(0),0,0x25)};
    if(m=="xor" )return{encR(reg(1),reg(2),reg(0),0,0x26)};
    if(m=="nor" )return{encR(reg(1),reg(2),reg(0),0,0x27)};
    if(m=="slt" )return{encR(reg(1),reg(2),reg(0),0,0x2A)};
    if(m=="sltu")return{encR(reg(1),reg(2),reg(0),0,0x2B)};

    // ── Shifts ──
    if(m=="sll" )return{encR(0,reg(1),reg(0),(int)parseNum(ops[2]),0x00)};
    if(m=="srl" )return{encR(0,reg(1),reg(0),(int)parseNum(ops[2]),0x02)};
    if(m=="sra" )return{encR(0,reg(1),reg(0),(int)parseNum(ops[2]),0x03)};
    if(m=="sllv")return{encR(reg(2),reg(1),reg(0),0,0x04)};
    if(m=="srlv")return{encR(reg(2),reg(1),reg(0),0,0x06)};
    if(m=="srav")return{encR(reg(2),reg(1),reg(0),0,0x07)};

    // ── Jump/link ──
    if(m=="jr"  )return{encR(reg(0),0,0,0,0x08)};
    if(m=="jalr"){
        int rd=31,rs=reg(0);
        if(ops.size()>=2){rd=reg(0);rs=reg(1);}
        return{encR(rs,0,rd,0,0x09)};
    }

    // ── HI/LO ──
    if(m=="mfhi")return{encR(0,0,reg(0),0,0x10)};
    if(m=="mthi")return{encR(reg(0),0,0,0,0x11)};
    if(m=="mflo")return{encR(0,0,reg(0),0,0x12)};
    if(m=="mtlo")return{encR(reg(0),0,0,0,0x13)};
    if(m=="mult" ||m=="multu"){ int fn=(m=="mult")?0x18:0x19; return{encR(reg(0),reg(1),0,0,fn)}; }
    if(m=="div"  ||m=="divu" ){ int fn=(m=="div") ?0x1A:0x1B; return{encR(reg(0),reg(1),0,0,fn)}; }

    // ── Syscall / break ──
    if(m=="syscall")return{0x0000000Cu};
    if(m=="break"  )return{0x0000000Du};
    if(m=="nop"    )return{0x00000000u};

    // ── I-type arithmetic ──
    if(m=="addi" )return{encI(0x08,reg(1),reg(0),(int16_t)immI(2))};
    if(m=="addiu")return{encI(0x09,reg(1),reg(0),(int16_t)immI(2))};
    if(m=="andi" )return{encI(0x0C,reg(1),reg(0),(int16_t)immI(2))};
    if(m=="ori"  )return{encI(0x0D,reg(1),reg(0),(int16_t)immI(2))};
    if(m=="xori" )return{encI(0x0E,reg(1),reg(0),(int16_t)immI(2))};
    if(m=="lui"  )return{encI(0x0F,0,     reg(0),(int16_t)immI(1))};
    if(m=="slti" )return{encI(0x0A,reg(1),reg(0),(int16_t)immI(2))};
    if(m=="sltiu")return{encI(0x0B,reg(1),reg(0),(int16_t)immI(2))};

    // ── Loads ──
    if(m=="lw" )return{encI(0x23,mBase(1),reg(0),mOff(1))};
    if(m=="lh" )return{encI(0x21,mBase(1),reg(0),mOff(1))};
    if(m=="lb" )return{encI(0x20,mBase(1),reg(0),mOff(1))};
    if(m=="lhu")return{encI(0x25,mBase(1),reg(0),mOff(1))};
    if(m=="lbu")return{encI(0x24,mBase(1),reg(0),mOff(1))};
    if(m=="lwl")return{encI(0x22,mBase(1),reg(0),mOff(1))};
    if(m=="lwr")return{encI(0x26,mBase(1),reg(0),mOff(1))};

    // ── Stores ──
    if(m=="sw" )return{encI(0x2B,mBase(1),reg(0),mOff(1))};
    if(m=="sh" )return{encI(0x29,mBase(1),reg(0),mOff(1))};
    if(m=="sb" )return{encI(0x28,mBase(1),reg(0),mOff(1))};
    if(m=="swl")return{encI(0x2A,mBase(1),reg(0),mOff(1))};
    if(m=="swr")return{encI(0x2E,mBase(1),reg(0),mOff(1))};

    // ── Branches ──
    if(m=="beq" )return{encI(0x04,reg(0),reg(1),(int16_t)immB(2))};
    if(m=="bne" )return{encI(0x05,reg(0),reg(1),(int16_t)immB(2))};
    if(m=="blez")return{encI(0x06,reg(0),0,     (int16_t)immB(1))};
    if(m=="bgtz")return{encI(0x07,reg(0),0,     (int16_t)immB(1))};
    if(m=="bltz")return{encI(0x01,reg(0),0x00,  (int16_t)immB(1))};
    if(m=="bgez")return{encI(0x01,reg(0),0x01,  (int16_t)immB(1))};
    if(m=="bltzal")return{encI(0x01,reg(0),0x10,(int16_t)immB(1))};
    if(m=="bgezal")return{encI(0x01,reg(0),0x11,(int16_t)immB(1))};

    // ── Jumps ──
    if(m=="j"  )return{encJ(0x02,jt(0))};
    if(m=="jal")return{encJ(0x03,jt(0))};

    // ── Pseudo-instructions ──────────────────────────────────────────

    // nop already handled above
    if(m=="move")return{encR(reg(1),0,reg(0),0,0x21)};   // addu rd,$0,rs
    if(m=="not" )return{encR(reg(1),0,reg(0),0,0x27)};   // nor  rd,rs,$0
    if(m=="neg" )return{encR(0,reg(1),reg(0),0,0x22)};   // sub  rd,$0,rs
    if(m=="negu")return{encR(0,reg(1),reg(0),0,0x23)};   // subu rd,$0,rs

    if(m=="mul"){
        // mul $rd, $rs, $rt  → mult $rs,$rt ; mflo $rd
        return { encR(reg(1),reg(2),0,0,0x18),          // mult rs,rt
                 encR(0,0,reg(0),0,0x12) };              // mflo rd
    }

    if(m=="li"){
        int32_t v=(int32_t)immI(1);
        if(v>=-32768&&v<=65535){
            // addiu rt,$0,v
            return{encI(0x09,0,reg(0),(int16_t)v)};
        } else {
            // lui rt, upper ; ori rt,rt, lower
            int16_t hi=(int16_t)(((uint32_t)v>>16)&0xFFFF);
            int16_t lo=(int16_t)(v&0xFFFF);
            return{ encI(0x0F,0,reg(0),hi),
                    encI(0x0D,reg(0),reg(0),lo) };
        }
    }

    if(m=="la"){
        // la $rt, label  →  lui $rt, upper(label) ; ori $rt,$rt,lower(label)
        int32_t addr=(int32_t)immI(1);
        int16_t hi=(int16_t)(((uint32_t)addr>>16)&0xFFFF);
        int16_t lo=(int16_t)(addr&0xFFFF);
        return{ encI(0x0F,0,reg(0),hi),
                encI(0x0D,reg(0),reg(0),lo) };
    }

    // ── Branch pseudo-instructions  (2 words each) ──────────────────
    // Supports both register and immediate second operand.
    // Register form:  blt $rs, $rt, lbl
    // Immediate form: blt $rs, 5,   lbl
    //
    // blt  rs, rt/imm, lbl  →  branch if rs <  rt/imm
    // bgt  rs, rt/imm, lbl  →  branch if rs >  rt/imm
    // ble  rs, rt/imm, lbl  →  branch if rs <= rt/imm
    // bge  rs, rt/imm, lbl  →  branch if rs >= rt/imm
    if(m=="blt"||m=="bgt"||m=="ble"||m=="bge"||m=="bltu"||m=="bgtu"){
        bool isImm = isNumStr(ops[1]);
        int16_t off = (int16_t)resolveImm(ops[2], pc+4, true, sym);
        bool isSigned = (m!="bltu" && m!="bgtu");
        uint8_t sltFn = isSigned ? 0x2A : 0x2B;   // slt / sltu
        uint8_t sltiOp = isSigned ? 0x0A : 0x0B;  // slti / sltiu

        if(isImm){
            int32_t imv = parseNum(ops[1]);
            // blt rs, imm  →  slti $at, rs, imm   ; bne $at, $0, lbl
            // bge rs, imm  →  slti $at, rs, imm   ; beq $at, $0, lbl
            // bgt rs, imm  →  slti $at, rs, imm+1 ; beq $at, $0, lbl  (rs > imm  ↔  rs >= imm+1)
            // ble rs, imm  →  slti $at, rs, imm+1 ; bne $at, $0, lbl  (rs <= imm ↔ !(rs >= imm+1))
            if(m=="blt"||m=="bltu")
                return{ encI(sltiOp,reg(0),1,(int16_t)imv),      // slti $at,rs,imv
                        encI(0x05,1,0,off) };                     // bne  $at,$0,lbl
            if(m=="bge"||m=="bgeu")
                return{ encI(sltiOp,reg(0),1,(int16_t)imv),
                        encI(0x04,1,0,off) };                     // beq  $at,$0,lbl
            if(m=="bgt"||m=="bgtu")
                return{ encI(sltiOp,reg(0),1,(int16_t)(imv+1)),  // slti $at,rs,imv+1
                        encI(0x04,1,0,off) };
            if(m=="ble"||m=="bleu")
                return{ encI(sltiOp,reg(0),1,(int16_t)(imv+1)),
                        encI(0x05,1,0,off) };
        } else {
            // register-register forms
            // blt rs,rt  →  slt  $at,rs,rt  ; bne $at,$0,lbl
            // bgt rs,rt  →  slt  $at,rt,rs  ; bne $at,$0,lbl
            // ble rs,rt  →  slt  $at,rt,rs  ; beq $at,$0,lbl
            // bge rs,rt  →  slt  $at,rs,rt  ; beq $at,$0,lbl
            if(m=="blt"||m=="bltu")
                return{ encR(reg(0),reg(1),1,0,sltFn),
                        encI(0x05,1,0,off) };
            if(m=="bgt"||m=="bgtu")
                return{ encR(reg(1),reg(0),1,0,sltFn),
                        encI(0x05,1,0,off) };
            if(m=="ble")
                return{ encR(reg(1),reg(0),1,0,sltFn),
                        encI(0x04,1,0,off) };
            if(m=="bge")
                return{ encR(reg(0),reg(1),1,0,sltFn),
                        encI(0x04,1,0,off) };
        }
    }

    throw std::runtime_error("unknown mnemonic: "+m);
}

// ── Two-pass assembler ────────────────────────────────────────────────
static AssembledProgram assemble(const std::string& source){
    AssembledProgram prog;
    prog.textBase = TEXT_BASE;
    prog.dataBase = DATA_BASE;

    std::vector<ParsedLine> lines;
    {
        std::istringstream ss(source);
        std::string line;
        while(std::getline(ss,line)) lines.push_back(parseLine(line));
    }

    // ── Pass 1: build symbol table ──────────────────────────────────
    SymTab sym;
    bool inData=false;
    uint32_t textOff=0, dataOff=0;

    for(auto& L:lines){
        if(L.isEmpty) continue;

        if(!L.label.empty()){
            uint32_t addr = inData ? (DATA_BASE+dataOff) : (TEXT_BASE+textOff*4);
            sym[L.label]=addr;
        }

        if(!L.directive.empty()){
            std::string d=L.directive;
            if(d==".text")  { inData=false; continue; }
            if(d==".data")  { inData=true;  continue; }
            if(d==".globl"||d==".global"||d==".ent"||d==".end"||d==".set"||d==".file") continue;
            if(inData){
                if(d==".word"){
                    dataOff += 4*(uint32_t)L.ops.size();
                } else if(d==".half"){
                    dataOff += 2*(uint32_t)L.ops.size();
                } else if(d==".byte"){
                    dataOff += 1*(uint32_t)L.ops.size();
                } else if(d==".space"){
                    if(!L.ops.empty()) dataOff+=(uint32_t)parseNum(L.ops[0]);
                } else if(d==".ascii"){
                    if(!L.ops.empty()) dataOff+=(uint32_t)parseStringLiteral(L.ops[0]).size();
                } else if(d==".asciiz"){
                    if(!L.ops.empty()) dataOff+=(uint32_t)(parseStringLiteral(L.ops[0]).size()+1);
                } else if(d==".align"){
                    if(!L.ops.empty()){
                        uint32_t n=1u<<(uint32_t)parseNum(L.ops[0]);
                        dataOff = (dataOff+n-1)&~(n-1);
                    }
                }
            }
            continue;
        }

        if(!L.mnem.empty() && !inData){
            textOff += (uint32_t)instrWordCount(L.mnem, L.ops);
        }
    }

    // ── Pass 2: emit code & data ────────────────────────────────────
    inData=false;
    uint32_t curTextPC=TEXT_BASE;

    for(auto& L:lines){
        if(L.isEmpty) continue;

        if(!L.directive.empty()){
            std::string d=L.directive;
            if(d==".text")  { inData=false; continue; }
            if(d==".data")  { inData=true;  continue; }
            if(d==".globl"||d==".global"||d==".ent"||d==".end"||d==".set"||d==".file") continue;
            if(inData){
                if(d==".word"){
                    for(auto& op:L.ops){
                        uint32_t v;
                        if(isNumStr(op)) v=(uint32_t)parseNum(op);
                        else{
                            auto it=sym.find(op);
                            v=(it!=sym.end())?(uint32_t)it->second:0;
                        }
                        prog.dataBytes.push_back((v>>24)&0xFF);
                        prog.dataBytes.push_back((v>>16)&0xFF);
                        prog.dataBytes.push_back((v>> 8)&0xFF);
                        prog.dataBytes.push_back( v     &0xFF);
                    }
                } else if(d==".half"){
                    for(auto& op:L.ops){
                        uint16_t v=(uint16_t)parseNum(op);
                        prog.dataBytes.push_back((v>>8)&0xFF);
                        prog.dataBytes.push_back( v    &0xFF);
                    }
                } else if(d==".byte"){
                    for(auto& op:L.ops){
                        prog.dataBytes.push_back((uint8_t)parseNum(op));
                    }
                } else if(d==".space"){
                    if(!L.ops.empty()){
                        int n=parseNum(L.ops[0]);
                        for(int i=0;i<n;i++) prog.dataBytes.push_back(0);
                    }
                } else if(d==".ascii"||d==".asciiz"){
                    if(!L.ops.empty()){
                        std::string s=parseStringLiteral(L.ops[0]);
                        for(char c:s) prog.dataBytes.push_back((uint8_t)c);
                        if(d==".asciiz") prog.dataBytes.push_back(0);
                    }
                } else if(d==".align"){
                    if(!L.ops.empty()){
                        uint32_t n=1u<<(uint32_t)parseNum(L.ops[0]);
                        while(prog.dataBytes.size()%n) prog.dataBytes.push_back(0);
                    }
                }
            }
            continue;
        }

        if(!L.mnem.empty() && !inData){
            try {
                auto words = encodeInstr(L.mnem, L.ops, curTextPC, sym);
                for(uint32_t w:words){
                    prog.textWords.push_back(w);
                    curTextPC+=4;
                }
            } catch(std::exception& e){
                throw std::runtime_error("Assembler error at '"+L.mnem+"': "+e.what());
            }
        }
    }

    return prog;
}

// ═══════════════════════════════════════════════════════════════════
//  SECTION 5 — Disassembler (for pipeline display)
// ═══════════════════════════════════════════════════════════════════
static std::string disasm(uint32_t instr){
    if(instr==0) return "nop";
    std::ostringstream o;
    uint8_t op=iOP(instr),rs=iRS(instr),rt=iRT(instr),rd=iRD(instr);
    uint8_t sh=iSH(instr),fn=iFN(instr);
    int32_t si=iSIMM(instr);
    auto R=[](uint8_t r)->const char*{ return RNAME[r&31]; };
    if(op==0){
        switch(fn){
            case 0x20:o<<"add  "<<R(rd)<<","<<R(rs)<<","<<R(rt);break;
            case 0x21:o<<"addu "<<R(rd)<<","<<R(rs)<<","<<R(rt);break;
            case 0x22:o<<"sub  "<<R(rd)<<","<<R(rs)<<","<<R(rt);break;
            case 0x23:o<<"subu "<<R(rd)<<","<<R(rs)<<","<<R(rt);break;
            case 0x24:o<<"and  "<<R(rd)<<","<<R(rs)<<","<<R(rt);break;
            case 0x25:o<<"or   "<<R(rd)<<","<<R(rs)<<","<<R(rt);break;
            case 0x26:o<<"xor  "<<R(rd)<<","<<R(rs)<<","<<R(rt);break;
            case 0x27:o<<"nor  "<<R(rd)<<","<<R(rs)<<","<<R(rt);break;
            case 0x2A:o<<"slt  "<<R(rd)<<","<<R(rs)<<","<<R(rt);break;
            case 0x2B:o<<"sltu "<<R(rd)<<","<<R(rs)<<","<<R(rt);break;
            case 0x00:o<<"sll  "<<R(rd)<<","<<R(rt)<<","<<(int)sh;break;
            case 0x02:o<<"srl  "<<R(rd)<<","<<R(rt)<<","<<(int)sh;break;
            case 0x03:o<<"sra  "<<R(rd)<<","<<R(rt)<<","<<(int)sh;break;
            case 0x04:o<<"sllv "<<R(rd)<<","<<R(rt)<<","<<R(rs);break;
            case 0x06:o<<"srlv "<<R(rd)<<","<<R(rt)<<","<<R(rs);break;
            case 0x07:o<<"srav "<<R(rd)<<","<<R(rt)<<","<<R(rs);break;
            case 0x08:o<<"jr   "<<R(rs);break;
            case 0x09:o<<"jalr "<<R(rd)<<","<<R(rs);break;
            case 0x10:o<<"mfhi "<<R(rd);break;
            case 0x11:o<<"mthi "<<R(rs);break;
            case 0x12:o<<"mflo "<<R(rd);break;
            case 0x13:o<<"mtlo "<<R(rs);break;
            case 0x18:o<<"mult "<<R(rs)<<","<<R(rt);break;
            case 0x19:o<<"multu "<<R(rs)<<","<<R(rt);break;
            case 0x1A:o<<"div  "<<R(rs)<<","<<R(rt);break;
            case 0x1B:o<<"divu "<<R(rs)<<","<<R(rt);break;
            case 0x0C:o<<"syscall";break;
            default:  o<<"r?(fn=0x"<<std::hex<<(int)fn<<")";break;
        }
    } else {
        switch(op){
            case 0x08:o<<"addi  "<<R(rt)<<","<<R(rs)<<","<<si;break;
            case 0x09:o<<"addiu "<<R(rt)<<","<<R(rs)<<","<<si;break;
            case 0x0C:o<<"andi  "<<R(rt)<<","<<R(rs)<<","<<si;break;
            case 0x0D:o<<"ori   "<<R(rt)<<","<<R(rs)<<","<<si;break;
            case 0x0E:o<<"xori  "<<R(rt)<<","<<R(rs)<<","<<si;break;
            case 0x0F:o<<"lui   "<<R(rt)<<","<<si;break;
            case 0x0A:o<<"slti  "<<R(rt)<<","<<R(rs)<<","<<si;break;
            case 0x0B:o<<"sltiu "<<R(rt)<<","<<R(rs)<<","<<si;break;
            case 0x23:o<<"lw    "<<R(rt)<<","<<si<<"("<<R(rs)<<")";break;
            case 0x21:o<<"lh    "<<R(rt)<<","<<si<<"("<<R(rs)<<")";break;
            case 0x20:o<<"lb    "<<R(rt)<<","<<si<<"("<<R(rs)<<")";break;
            case 0x25:o<<"lhu   "<<R(rt)<<","<<si<<"("<<R(rs)<<")";break;
            case 0x24:o<<"lbu   "<<R(rt)<<","<<si<<"("<<R(rs)<<")";break;
            case 0x22:o<<"lwl   "<<R(rt)<<","<<si<<"("<<R(rs)<<")";break;
            case 0x26:o<<"lwr   "<<R(rt)<<","<<si<<"("<<R(rs)<<")";break;
            case 0x2B:o<<"sw    "<<R(rt)<<","<<si<<"("<<R(rs)<<")";break;
            case 0x29:o<<"sh    "<<R(rt)<<","<<si<<"("<<R(rs)<<")";break;
            case 0x28:o<<"sb    "<<R(rt)<<","<<si<<"("<<R(rs)<<")";break;
            case 0x2A:o<<"swl   "<<R(rt)<<","<<si<<"("<<R(rs)<<")";break;
            case 0x2E:o<<"swr   "<<R(rt)<<","<<si<<"("<<R(rs)<<")";break;
            case 0x04:o<<"beq   "<<R(rs)<<","<<R(rt)<<","<<si;break;
            case 0x05:o<<"bne   "<<R(rs)<<","<<R(rt)<<","<<si;break;
            case 0x06:o<<"blez  "<<R(rs)<<","<<si;break;
            case 0x07:o<<"bgtz  "<<R(rs)<<","<<si;break;
            case 0x01:{
                const char* nm[]={"bltz","bgez","?","?","?","?","?","?",
                                  "?","?","?","?","?","?","?","?",
                                  "bltzal","bgezal"};
                o<<(rt<=0x11?nm[rt]:"regimm")<<" "<<R(rs)<<","<<si;break;
            }
            case 0x02:o<<"j     0x"<<std::hex<<(iJTGT(instr)<<2);break;
            case 0x03:o<<"jal   0x"<<std::hex<<(iJTGT(instr)<<2);break;
            default:  o<<"?(op=0x"<<std::hex<<(int)op<<")";break;
        }
    }
    return o.str();
}

// ═══════════════════════════════════════════════════════════════════
//  SECTION 6 — Pipeline structures
// ═══════════════════════════════════════════════════════════════════
struct CtrlSig {
    bool regDst=false,aluSrc=false,memToReg=false;
    bool regWrite=false,memRead=false,memWrite=false;
    bool branch=false,jump=false,jumpReg=false,link=false;
    bool isNop=true;
    enum class ALUOp:uint8_t{
        ADD=0,SUB,AND,OR,XOR,NOR,SLT,SLTU,
        SLL,SRL,SRA,SLLV,SRLV,SRAV,LUI,PASSB
    } aluOp=ALUOp::ADD;
};

struct IF_ID  { uint32_t pc=0,instr=0; bool valid=false; };
struct ID_EX  {
    uint32_t pc=0,instr=0,regA=0,regB=0;
    int32_t  imm=0;
    uint8_t  rs=0,rt=0,rd=0,shamt=0;
    uint32_t jumpTarget=0;
    CtrlSig  ctrl;
    bool     valid=false;
};
struct EX_MEM {
    uint32_t pc=0,instr=0,aluResult=0,regB=0;
    uint8_t  destReg=0;
    CtrlSig  ctrl;
    bool     valid=false;
};
struct MEM_WB {
    uint32_t pc=0,instr=0,aluResult=0,memData=0;
    uint8_t  destReg=0;
    CtrlSig  ctrl;
    bool     valid=false;
};
enum class FwdSrc{NONE,EX_MEM,MEM_WB};

// ═══════════════════════════════════════════════════════════════════
//  SECTION 7 — MIPSPipeline class
// ═══════════════════════════════════════════════════════════════════
class MIPSPipeline {
public:
    uint32_t regs[32]={};
    uint32_t hi=0,lo=0,pc=0;
    uint8_t  mem[MEM_SIZE]={};

    IF_ID   if_id,if_id_n;
    ID_EX   id_ex,id_ex_n;
    EX_MEM  ex_mem,ex_mem_n;
    MEM_WB  mem_wb,mem_wb_n;

    bool     stall=false,branchTaken=false,halted=false;
    uint32_t branchTarget=0;
    uint64_t cycles=0,instrRet=0,nStalls=0,nFlushes=0;
    bool     verbose=false;

    // Memory I/O
    inline uint32_t pa(uint32_t va){ return va & MEM_MASK; }
    uint32_t  rdW(uint32_t a){ a=pa(a); return ((uint32_t)mem[a]<<24)|((uint32_t)mem[a+1]<<16)|((uint32_t)mem[a+2]<<8)|mem[a+3]; }
    uint16_t  rdH(uint32_t a){ a=pa(a); return ((uint16_t)mem[a]<<8)|mem[a+1]; }
    uint8_t   rdB(uint32_t a){ return mem[pa(a)]; }
    void wrW(uint32_t a,uint32_t v){ a=pa(a); mem[a]=(v>>24)&0xFF;mem[a+1]=(v>>16)&0xFF;mem[a+2]=(v>>8)&0xFF;mem[a+3]=v&0xFF; }
    void wrH(uint32_t a,uint16_t v){ a=pa(a); mem[a]=(v>>8)&0xFF; mem[a+1]=v&0xFF; }
    void wrB(uint32_t a,uint8_t v){ mem[pa(a)]=v; }

    void load(const AssembledProgram& prog){
        pc = prog.textBase;
        // text
        for(size_t i=0;i<prog.textWords.size();i++)
            wrW(prog.textBase + (uint32_t)(i*4), prog.textWords[i]);
        // data
        for(size_t i=0;i<prog.dataBytes.size();i++)
            mem[pa(prog.dataBase+i)]=prog.dataBytes[i];
        // stack pointer
        regs[29] = 0x7FFFEFFC & MEM_MASK;
        regs[28] = pa(prog.dataBase); // $gp
    }

    // ── Control signals ────────────────────────────────────────────
    CtrlSig decodeCtrl(uint32_t instr){
        CtrlSig c;
        if(instr==0){ c.isNop=true; return c; }
        c.isNop=false;
        uint8_t op=iOP(instr),fn=iFN(instr);
        using A=CtrlSig::ALUOp;
        switch(op){
            case 0x00: // R-type
                c.regDst=true; c.regWrite=true;
                switch(fn){
                    case 0x20:case 0x21: c.aluOp=A::ADD;  break;
                    case 0x22:case 0x23: c.aluOp=A::SUB;  break;
                    case 0x24: c.aluOp=A::AND;  break;
                    case 0x25: c.aluOp=A::OR;   break;
                    case 0x26: c.aluOp=A::XOR;  break;
                    case 0x27: c.aluOp=A::NOR;  break;
                    case 0x2A: c.aluOp=A::SLT;  break;
                    case 0x2B: c.aluOp=A::SLTU; break;
                    case 0x00: c.aluOp=A::SLL;  break;
                    case 0x02: c.aluOp=A::SRL;  break;
                    case 0x03: c.aluOp=A::SRA;  break;
                    case 0x04: c.aluOp=A::SLLV; break;
                    case 0x06: c.aluOp=A::SRLV; break;
                    case 0x07: c.aluOp=A::SRAV; break;
                    case 0x08: c.jump=true; c.jumpReg=true; c.regWrite=false; break;
                    case 0x09: c.jump=true; c.jumpReg=true; c.link=true;      break;
                    case 0x10:case 0x12: c.aluOp=A::PASSB; break; // mfhi/mflo
                    case 0x18:case 0x19:case 0x1A:case 0x1B:
                    case 0x11:case 0x13: c.regWrite=false; break;
                    case 0x0C: c.regWrite=false; break; // syscall
                    default:   c.regWrite=false; break;
                }
                break;
            case 0x08:case 0x09: c.aluSrc=true;c.regWrite=true;c.aluOp=A::ADD;  break;
            case 0x0C:           c.aluSrc=true;c.regWrite=true;c.aluOp=A::AND;  break;
            case 0x0D:           c.aluSrc=true;c.regWrite=true;c.aluOp=A::OR;   break;
            case 0x0E:           c.aluSrc=true;c.regWrite=true;c.aluOp=A::XOR;  break;
            case 0x0F:           c.aluSrc=true;c.regWrite=true;c.aluOp=A::LUI;  break;
            case 0x0A:           c.aluSrc=true;c.regWrite=true;c.aluOp=A::SLT;  break;
            case 0x0B:           c.aluSrc=true;c.regWrite=true;c.aluOp=A::SLTU; break;
            case 0x23:case 0x21:case 0x20:case 0x25:case 0x24:case 0x22:case 0x26:
                c.aluSrc=true;c.memRead=true;c.regWrite=true;c.memToReg=true;c.aluOp=A::ADD; break;
            case 0x2B:case 0x29:case 0x28:case 0x2A:case 0x2E:
                c.aluSrc=true;c.memWrite=true;c.aluOp=A::ADD; break;
            case 0x04:case 0x05:case 0x06:case 0x07:case 0x01:
                c.branch=true;c.aluOp=A::SUB; break;
            case 0x02: c.jump=true; break;
            case 0x03: c.jump=true;c.link=true;c.regWrite=true; break;
            default: break;
        }
        return c;
    }

    // ── ALU ────────────────────────────────────────────────────────
    uint32_t alu(CtrlSig::ALUOp op,uint32_t a,uint32_t b,uint8_t sh=0){
        using A=CtrlSig::ALUOp;
        switch(op){
            case A::ADD:  return a+b;
            case A::SUB:  return a-b;
            case A::AND:  return a&b;
            case A::OR:   return a|b;
            case A::XOR:  return a^b;
            case A::NOR:  return ~(a|b);
            case A::SLT:  return (int32_t)a<(int32_t)b?1:0;
            case A::SLTU: return a<b?1:0;
            case A::SLL:  return b<<sh;
            case A::SRL:  return b>>sh;
            case A::SRA:  return (uint32_t)((int32_t)b>>sh);
            case A::SLLV: return b<<(a&31);
            case A::SRLV: return b>>(a&31);
            case A::SRAV: return (uint32_t)((int32_t)b>>(a&31));
            case A::LUI:  return b<<16;
            case A::PASSB:return b;
            default:      return 0;
        }
    }

    // ── Forwarding unit ─────────────────────────────────────────────
    FwdSrc fwdA(uint8_t rs){
        if(ex_mem.ctrl.regWrite&&ex_mem.destReg!=0&&ex_mem.destReg==rs) return FwdSrc::EX_MEM;
        if(mem_wb.ctrl.regWrite&&mem_wb.destReg!=0&&mem_wb.destReg==rs) return FwdSrc::MEM_WB;
        return FwdSrc::NONE;
    }
    FwdSrc fwdB(uint8_t rt){
        if(ex_mem.ctrl.regWrite&&ex_mem.destReg!=0&&ex_mem.destReg==rt) return FwdSrc::EX_MEM;
        if(mem_wb.ctrl.regWrite&&mem_wb.destReg!=0&&mem_wb.destReg==rt) return FwdSrc::MEM_WB;
        return FwdSrc::NONE;
    }
    uint32_t resolveFwd(FwdSrc src,uint32_t reg){
        if(src==FwdSrc::EX_MEM) return ex_mem.aluResult;
        if(src==FwdSrc::MEM_WB) return mem_wb.ctrl.memToReg?mem_wb.memData:mem_wb.aluResult;
        return reg;
    }

    // ── Hazard detection (load-use) ─────────────────────────────────
    bool loadUseHazard(){
        if(!id_ex.ctrl.memRead) return false;
        uint8_t rs=iRS(if_id.instr),rt=iRT(if_id.instr);
        return id_ex.rt==rs||id_ex.rt==rt;
    }

    // ── Syscall ─────────────────────────────────────────────────────
    void doSyscall(){
        uint32_t code=regs[2]; // $v0
        switch(code){
            case 1:  std::cout<<(int32_t)regs[4];                          break; // print_int
            case 2:{ float fv; std::memcpy(&fv,&regs[6],4); std::cout<<fv; break; }
            case 4:{                                                               // print_string
                uint32_t a=regs[4];
                while(mem[pa(a)]) std::cout<<(char)mem[pa(a++)];
                break;
            }
            case 5:{ int v; std::cin>>v; regs[2]=(uint32_t)v; break; }           // read_int
            case 8:{                                                               // read_string
                std::string line; std::getline(std::cin,line);
                uint32_t buf=regs[4]; uint32_t maxlen=regs[5];
                for(uint32_t i=0;i<maxlen-1&&i<line.size();i++)
                    mem[pa(buf+i)]=(uint8_t)line[i];
                mem[pa(buf+std::min((uint32_t)line.size(),maxlen-1))]=0;
                break;
            }
            case 10: halted=true;                                          break; // exit
            case 11: std::cout<<(char)regs[4];                            break; // print_char
            case 12:{ char c; std::cin>>c; regs[2]=(uint32_t)c; break; }        // read_char
            case 13:{                                                             // open (stub)
                regs[2]=(uint32_t)-1; break;
            }
            case 17: halted=true;                                          break; // exit2
            default:
                std::cerr<<"[syscall "<<code<<" not implemented]\n"; break;
        }
    }

    // ── Stage IF ────────────────────────────────────────────────────
    void stageIF(){
        if(stall){ if_id_n=if_id; return; }
        if(branchTaken){
            if_id_n={branchTarget,0,false};
            pc=branchTarget; branchTaken=false; return;
        }
        if_id_n={pc, rdW(pc), true};
        pc+=4;
    }

    // ── Stage ID ────────────────────────────────────────────────────
    void stageID(){
        if(stall){ id_ex_n={}; id_ex_n.ctrl.isNop=true; nStalls++; return; }
        // If branch/jump was resolved in EX this same cycle, squash ID output
        if(branchTaken){ id_ex_n={}; id_ex_n.ctrl.isNop=true; return; }
        uint32_t instr=if_id.instr;
        id_ex_n.pc=if_id.pc; id_ex_n.instr=instr; id_ex_n.valid=if_id.valid;
        if(!if_id.valid||instr==0){ id_ex_n.ctrl.isNop=true; return; }
        id_ex_n.rs=iRS(instr); id_ex_n.rt=iRT(instr);
        id_ex_n.rd=iRD(instr); id_ex_n.shamt=iSH(instr);
        id_ex_n.imm=iSIMM(instr);
        id_ex_n.regA=regs[iRS(instr)];
        id_ex_n.regB=regs[iRT(instr)];
        id_ex_n.ctrl=decodeCtrl(instr);
        id_ex_n.jumpTarget=((if_id.pc+4)&0xF0000000u)|(iJTGT(instr)<<2);
    }

    // ── Stage EX ────────────────────────────────────────────────────
    void stageEX(){
        if(!id_ex.valid||id_ex.ctrl.isNop){ ex_mem_n={}; ex_mem_n.ctrl.isNop=true; return; }

        uint32_t valA=resolveFwd(fwdA(id_ex.rs),id_ex.regA);
        uint32_t valB=resolveFwd(fwdB(id_ex.rt),id_ex.regB);

        uint32_t aluB=id_ex.ctrl.aluSrc?(uint32_t)id_ex.imm:valB;
        // Zero-extend for logical immediate ops
        uint8_t op=iOP(id_ex.instr);
        if(op==0x0C||op==0x0D||op==0x0E) aluB=(uint32_t)(uint16_t)id_ex.imm;

        uint32_t result=0;
        uint8_t fn=iFN(id_ex.instr);
        if(op==0){
            if(fn==0x18||fn==0x19){
                int64_t p=(fn==0x18)?(int64_t)(int32_t)valA*(int32_t)valB
                                    :(int64_t)(uint64_t)valA*(uint64_t)valB;
                lo=(uint32_t)(p&0xFFFFFFFFu); hi=(uint32_t)(p>>32);
            } else if(fn==0x1A||fn==0x1B){
                if(valB){
                    if(fn==0x1A){ lo=(uint32_t)((int32_t)valA/(int32_t)valB); hi=(uint32_t)((int32_t)valA%(int32_t)valB); }
                    else         { lo=valA/valB; hi=valA%valB; }
                }
            } else if(fn==0x10) result=hi;
            else if(fn==0x12)   result=lo;
            else if(fn==0x11)   hi=valA;
            else if(fn==0x13)   lo=valA;
            else result=alu(id_ex.ctrl.aluOp,valA,aluB,id_ex.shamt);
        } else {
            result=alu(id_ex.ctrl.aluOp,valA,aluB,id_ex.shamt);
        }

        uint8_t dest=id_ex.ctrl.regDst?id_ex.rd:id_ex.rt;
        if(id_ex.ctrl.link) dest=(id_ex.ctrl.jumpReg)?id_ex.rd:31;

        // Branch resolution
        if(id_ex.ctrl.branch){
            bool taken=false;
            uint32_t brT=id_ex.pc+4+(id_ex.imm<<2);
            uint8_t brt=iRT(id_ex.instr);
            bool eq=(valA==valB),lt=((int32_t)valA<0),gt=((int32_t)valA>0);
            if(op==0x04)      taken=eq;
            else if(op==0x05) taken=!eq;
            else if(op==0x06) taken=(lt||eq);
            else if(op==0x07) taken=gt;
            else if(op==0x01){
                if(brt==0x00||brt==0x10)       taken=lt;
                else if(brt==0x01||brt==0x11)  taken=!lt;
                if((brt==0x10||brt==0x11)&&taken){ result=id_ex.pc+4; dest=31; id_ex_n.ctrl.regWrite=true; }
            }
            if(taken){ branchTaken=true; branchTarget=brT; nFlushes++; }
        }
        if(id_ex.ctrl.jump){
            branchTaken=true;
            branchTarget=id_ex.ctrl.jumpReg?valA:id_ex.jumpTarget;
            if(id_ex.ctrl.link) result=id_ex.pc+4;  // return addr = instr after jal
            nFlushes++;
        }

        ex_mem_n.pc=id_ex.pc; ex_mem_n.instr=id_ex.instr;
        ex_mem_n.aluResult=result; ex_mem_n.regB=valB;
        ex_mem_n.destReg=dest; ex_mem_n.ctrl=id_ex.ctrl;
        ex_mem_n.valid=true; ex_mem_n.ctrl.isNop=false;
    }

    // ── Stage MEM ───────────────────────────────────────────────────
    void stageMEM(){
        if(!ex_mem.valid||ex_mem.ctrl.isNop){ mem_wb_n={}; mem_wb_n.ctrl.isNop=true; return; }
        uint32_t mdata=0,maddr=ex_mem.aluResult;
        uint8_t op=iOP(ex_mem.instr);
        if(ex_mem.ctrl.memRead){
            switch(op){
                case 0x23: mdata=rdW(maddr); break;
                case 0x21: mdata=(uint32_t)(int32_t)(int16_t)rdH(maddr); break;
                case 0x25: mdata=(uint32_t)rdH(maddr); break;
                case 0x20: mdata=(uint32_t)(int32_t)(int8_t)rdB(maddr);  break;
                case 0x24: mdata=(uint32_t)rdB(maddr);  break;
                default:   mdata=rdW(maddr); break;
            }
        }
        if(ex_mem.ctrl.memWrite){
            switch(op){
                case 0x2B: wrW(maddr,ex_mem.regB); break;
                case 0x29: wrH(maddr,(uint16_t)ex_mem.regB); break;
                case 0x28: wrB(maddr,(uint8_t) ex_mem.regB); break;
                default:   wrW(maddr,ex_mem.regB); break;
            }
        }
        mem_wb_n.pc=ex_mem.pc; mem_wb_n.instr=ex_mem.instr;
        mem_wb_n.aluResult=ex_mem.aluResult; mem_wb_n.memData=mdata;
        mem_wb_n.destReg=ex_mem.destReg; mem_wb_n.ctrl=ex_mem.ctrl;
        mem_wb_n.valid=true; mem_wb_n.ctrl.isNop=false;
    }

    // ── Stage WB ────────────────────────────────────────────────────
    void stageWB(){
        if(!mem_wb.valid||mem_wb.ctrl.isNop) return;
        if(mem_wb.ctrl.regWrite&&mem_wb.destReg!=0){
            regs[mem_wb.destReg]=mem_wb.ctrl.memToReg?mem_wb.memData:mem_wb.aluResult;
        }
        regs[0]=0;
        // Syscall: all preceding register writes are now committed
        if(iOP(mem_wb.instr)==0&&iFN(mem_wb.instr)==0x0C) doSyscall();
        instrRet++;
    }

    // ── Verbose pipeline display ────────────────────────────────────
    void printPipeline(){
        auto fmtPC=[](uint32_t pc)->std::string{
            std::ostringstream s; s<<"0x"<<std::hex<<std::setw(8)<<std::setfill('0')<<pc; return s.str();
        };
        auto fmtInstr=[&](uint32_t addr, uint32_t instr, bool valid)->std::string{
            if(!valid||instr==0) return "<bubble>";
            return fmtPC(addr)+": "+disasm(instr);
        };
        std::cerr<<"\n-- Cycle "<<std::dec<<cycles<<" PC="<<fmtPC(pc)<<" --\n";
        std::cerr<<"  IF  | "<<fmtPC(pc)<<": (fetching)\n";
        std::cerr<<"  ID  | "<<fmtInstr(if_id.pc,if_id.instr,if_id.valid)<<"\n";
        std::cerr<<"  EX  | "<<(id_ex.valid&&!id_ex.ctrl.isNop?fmtPC(id_ex.pc)+": "+disasm(id_ex.instr):"<bubble>")<<"\n";
        std::cerr<<"  MEM | "<<(ex_mem.valid&&!ex_mem.ctrl.isNop?fmtPC(ex_mem.pc)+": "+disasm(ex_mem.instr):"<bubble>")<<"\n";
        std::cerr<<"  WB  | "<<(mem_wb.valid&&!mem_wb.ctrl.isNop?fmtPC(mem_wb.pc)+": "+disasm(mem_wb.instr):"<bubble>")<<"\n";
        if(stall)       std::cerr<<"  [STALL: load-use hazard]\n";
        if(branchTaken) std::cerr<<"  [BRANCH TAKEN → "<<fmtPC(branchTarget)<<"]\n";
    }

    // ── One clock tick ───────────────────────────────────────────────
    void tick(){
        if(halted) return;
        cycles++;
        stall=loadUseHazard();
        if(verbose) printPipeline();
        stageWB();
        stageMEM();
        stageEX();
        stageID();
        stageIF();
        mem_wb=mem_wb_n;
        ex_mem=ex_mem_n;
        id_ex=id_ex_n;
        if(!stall) if_id=if_id_n;
    }

    void run(uint64_t maxCycles=2000000){
        for(uint64_t c=0;c<maxCycles&&!halted;c++){
            tick();
            // Drain pipeline after program ends
            if(halted){
                for(int i=0;i<4&&!halted;i++) tick();
                break;
            }
        }
    }

    // ── Register / stats dump ────────────────────────────────────────
    void dumpRegisters(std::ostream& out) const {
        out << "=== Final Register File ===\n";
        for(int i=0;i<32;i++){
            out << std::setw(6) << RNAME[i] << " = "
                << "0x"<<std::hex<<std::setw(8)<<std::setfill('0')<<regs[i]
                << "  ("<<std::dec<<(int32_t)regs[i]<<")\n";
        }
        out <<"    HI = 0x"<<std::hex<<std::setw(8)<<std::setfill('0')<<hi<<"\n";
        out <<"    LO = 0x"<<std::hex<<std::setw(8)<<std::setfill('0')<<lo<<"\n";
        out <<"\n=== Pipeline Statistics ===\n";
        out <<"  Cycles         : "<<std::dec<<cycles<<"\n";
        out <<"  Instrs Retired : "<<instrRet<<"\n";
        out <<"  Stall Cycles   : "<<nStalls<<"\n";
        out <<"  Flush Cycles   : "<<nFlushes<<"\n";
        if(cycles>0) out <<"  IPC            : "<<std::fixed<<std::setprecision(3)<<(double)instrRet/cycles<<"\n";
    }
};

// ═══════════════════════════════════════════════════════════════════
//  SECTION 8 — Loaders
// ═══════════════════════════════════════════════════════════════════
static AssembledProgram loadBinary(const std::string& path){
    AssembledProgram prog;
    prog.textBase=TEXT_BASE;
    prog.dataBase=DATA_BASE;
    std::ifstream f(path,std::ios::binary);
    if(!f) throw std::runtime_error("cannot open binary: "+path);
    uint8_t buf[4];
    while(f.read((char*)buf,4))
        prog.textWords.push_back(((uint32_t)buf[0]<<24)|((uint32_t)buf[1]<<16)|((uint32_t)buf[2]<<8)|buf[3]);
    return prog;
}

static AssembledProgram loadHex(const std::string& path){
    AssembledProgram prog;
    prog.textBase=TEXT_BASE; prog.dataBase=DATA_BASE;
    std::ifstream f(path);
    if(!f) throw std::runtime_error("cannot open hex file: "+path);
    std::string line;
    while(std::getline(f,line)){
        line=trim(line);
        if(line.empty()||line[0]=='#') continue;
        prog.textWords.push_back((uint32_t)std::stoul(line,nullptr,16));
    }
    return prog;
}

static AssembledProgram loadAssembly(const std::string& path){
    std::ifstream f(path);
    if(!f) throw std::runtime_error("cannot open assembly file: "+path);
    std::string src((std::istreambuf_iterator<char>(f)),std::istreambuf_iterator<char>());
    return assemble(src);
}

// ═══════════════════════════════════════════════════════════════════
//  SECTION 9 — main
// ═══════════════════════════════════════════════════════════════════
static void usage(const char* prog){
    std::cerr
        <<"Usage: "<<prog<<" [options] <input_file>\n"
        <<"  input_file     : .s/.asm = MIPS assembly, .bin = raw binary, .hex = hex words\n"
        <<"  --log <path>   : write register dump to file (default: registers.log)\n"
        <<"  --stderr       : send register dump to stderr (no file)\n"
        <<"  --verbose      : print pipeline state each cycle (to stderr)\n"
        <<"  --max <n>      : cycle limit (default: 2000000)\n";
}

int main(int argc, char* argv[]){
    if(argc<2){ usage(argv[0]); return 1; }

    std::string inputFile;
    std::string logFile = "registers.log";
    bool toStderr = false;
    bool verbose  = false;
    uint64_t maxCycles = 2000000;

    for(int i=1;i<argc;i++){
        std::string a=argv[i];
        if(a=="--stderr"){ toStderr=true; }
        else if(a=="--verbose"){ verbose=true; }
        else if(a=="--log"&&i+1<argc){ logFile=argv[++i]; }
        else if(a=="--max"&&i+1<argc){ maxCycles=(uint64_t)std::stoull(argv[++i]); }
        else if(a[0]=='-'){ std::cerr<<"Unknown option: "<<a<<"\n"; usage(argv[0]); return 1; }
        else { inputFile=a; }
    }

    if(inputFile.empty()){ std::cerr<<"No input file specified.\n"; usage(argv[0]); return 1; }

    // ── Determine file type ────────────────────────────────────────
    std::string ext;
    {
        size_t dot=inputFile.rfind('.');
        if(dot!=std::string::npos) ext=toLower(inputFile.substr(dot));
    }

    AssembledProgram prog;
    try {
        if(ext==".bin")        prog=loadBinary(inputFile);
        else if(ext==".hex")   prog=loadHex(inputFile);
        else                   prog=loadAssembly(inputFile); // .s .asm or anything else
    } catch(std::exception& e){
        std::cerr<<"Load error: "<<e.what()<<"\n"; return 1;
    }

    if(prog.textWords.empty()){
        std::cerr<<"Warning: empty program (no .text instructions found).\n";
    }

    // ── Run ────────────────────────────────────────────────────────
    MIPSPipeline cpu;
    cpu.verbose=verbose;
    cpu.load(prog);

    try {
        cpu.run(maxCycles);
    } catch(std::exception& e){
        std::cerr<<"Runtime error: "<<e.what()<<"\n";
    }

    // ── Dump registers ─────────────────────────────────────────────
    if(toStderr){
        cpu.dumpRegisters(std::cerr);
    } else {
        std::ofstream lf(logFile);
        if(!lf){
            std::cerr<<"Warning: cannot open log file '"<<logFile<<"', using stderr.\n";
            cpu.dumpRegisters(std::cerr);
        } else {
            cpu.dumpRegisters(lf);
            std::cerr<<"[Register dump written to: "<<logFile<<"]\n";
        }
    }
    return 0;
}
