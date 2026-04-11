#ifndef LARGE_SHADER_H
#define LARGE_SHADER_H

#define STRINGIFY(...) #__VA_ARGS__
const char *large_frag = STRINGIFY(
FRAG
PROPERTY FS_COORD_ORIGIN LOWER_LEFT
DCL IN[0], POSITION, LINEAR
DCL OUT[0], COLOR
DCL CONST[1..2]
DCL CONST[0]
DCL TEMP[0]
DCL TEMP[1..258], LOCAL
IMM[0] FLT32 {0x40000000, 0xbf800000, 0x00000000, 0x3f7fffff}
IMM[1] FLT32 {0xbf3504f4, 0x3f3504f4, 0x00000000, 0x3fb504f3}
IMM[2] FLT32 {0xbf87c3b7, 0x80000000, 0xbf733333, 0x41000000}
IMM[3] UINT32 {0, 4294967295, 0, 0}
IMM[4] INT32 {0, 32, 1, 0}
IMM[5] FLT32 {0x3f6e147b, 0x00000000, 0x40400000, 0x42e20000}
IMM[6] FLT32 {0x42640000, 0x3f000000, 0x472aee8c, 0x3f800000}
IMM[7] FLT32 {0x42680000, 0x42e40000, 0x432a0000, 0x432b0000}
IMM[8] FLT32 {0xbf19999a, 0xbef5c28f, 0x3f23d70a, 0x400147ae}
IMM[9] FLT32 {0xbf4ccccd, 0x3eb851ec, 0xbef5c28f, 0x3e800000}
IMM[10] FLT32 {0x00000000, 0x3f4ccccd, 0x3f19999a, 0x4001eb85}
IMM[11] FLT32 {0x3e000000, 0x4000a3d7, 0x3d800000, 0x3d000000}
IMM[12] FLT32 {0x3f888889, 0xbe99999a, 0x3d4ccccd, 0x3f3851ec}
IMM[13] FLT32 {0x3ecccccd, 0x3f800000, 0x402ccccd, 0xbf000000}
IMM[14] FLT32 {0x3f7d70a4, 0x3eaaaa9f, 0x40400000, 0x3f800000}
IMM[15] FLT32 {0x3f800000, 0x3f000000, 0x00000000, 0x3f2aaa9f}
IMM[16] FLT32 {0x3e4ccccd, 0x3f800000, 0xbea3d70b, 0x3a83126f}
IMM[17] FLT32 {0x3eb33333, 0x3f59999a, 0x00000000, 0x00000000}
  0: MOV TEMP[0], IN[0]
  1: MAD TEMP[0].y, IN[0], CONST[2].zzzz, CONST[2].wwww
  2: RCP TEMP[1].x, CONST[0].xxxx
  3: RCP TEMP[1].y, CONST[0].yyyy
  4: MUL TEMP[1].xy, TEMP[0].xyyy, TEMP[1].xyyy
  5: MAD TEMP[1].xy, IMM[0].xxxx, TEMP[1].xyyy, IMM[0].yyyy
  6: MOV TEMP[2].y, TEMP[1].yyyy
  7: RCP TEMP[3].x, CONST[0].yyyy
  8: MUL TEMP[3].x, CONST[0].xxxx, TEMP[3].xxxx
  9: MUL TEMP[2].x, TEMP[1].xxxx, TEMP[3].xxxx
 10: MUL TEMP[2].xyz, TEMP[2].xxxx, IMM[0].zzww
 11: MAD TEMP[1].xyz, TEMP[1].yyyy, IMM[1].xyzz, TEMP[2].xyzz
 12: ADD TEMP[1].xyz, TEMP[1].xyzz, IMM[2].xxyy
 13: DP3 TEMP[2].x, TEMP[1].xyzz, TEMP[1].xyzz
 14: RSQ TEMP[2].x, TEMP[2].xxxx
 15: MUL TEMP[1].xyz, TEMP[1].xyzz, TEMP[2].xxxx
 16: MOV TEMP[2].xyz, IMM[1].wwzw
 17: MOV TEMP[3].x, IMM[3].xxxx
 18: MOV TEMP[4].x, IMM[0].zzzz
 19: MOV TEMP[5].x, IMM[4].xxxx
 20: BGNLOOP :2
 21:   ISGE TEMP[6].x, TEMP[5].xxxx, IMM[4].yyyy
 22:   UIF TEMP[6].xxxx :2
 23:     BRK
 24:   ENDIF
 25:   DP3 TEMP[7].x, TEMP[2].xyzz, TEMP[2].xyzz
 26:   RSQ TEMP[8].x, TEMP[7].xxxx
 27:   MUL TEMP[8].x, TEMP[8].xxxx, TEMP[7].xxxx
 28:   CMP TEMP[8].x, -TEMP[7].xxxx, TEMP[8].xxxx, IMM[0].zzzz
 29:   ADD TEMP[9].x, TEMP[8].xxxx, IMM[2].zzzz
 30:   MUL TEMP[10].xyz, TEMP[2].xyzz, IMM[2].wwww
 31:   MAD TEMP[11].xyz, IMM[5].xyyy, CONST[1].xxxx, TEMP[10].xyzz
 32:   FLR TEMP[12].xyz, TEMP[11].xyzz
 33:   FRC TEMP[13].xyz, TEMP[11].xyzz
 34:   MUL TEMP[14].xyz, TEMP[13].xyzz, TEMP[13].xyzz
 35:   MUL TEMP[15].xyz, IMM[0].xxxx, TEMP[13].xyzz
 36:   ADD TEMP[16].xyz, IMM[5].zzzz, -TEMP[15].xyzz
 37:   MUL TEMP[17].xyz, TEMP[14].xyzz, TEMP[16].xyzz
 38:   MAD TEMP[18].x, TEMP[12].yyyy, IMM[6].xxxx, TEMP[12].xxxx
 39:   MAD TEMP[19].x, IMM[5].wwww, TEMP[12].zzzz, TEMP[18].xxxx
 40:   SIN TEMP[20].x, TEMP[19].xxxx
 41:   MUL TEMP[21].x, TEMP[20].xxxx, IMM[6].zzzz
 42:   FRC TEMP[22].x, TEMP[21].xxxx
 43:   ADD TEMP[23].x, TEMP[19].xxxx, IMM[6].wwww
 44:   SIN TEMP[24].x, TEMP[23].xxxx
 45:   MUL TEMP[25].x, TEMP[24].xxxx, IMM[6].zzzz
 46:   FRC TEMP[26].x, TEMP[25].xxxx
 47:   LRP TEMP[27].x, TEMP[17].xxxx, TEMP[26].xxxx, TEMP[22].xxxx
 48:   ADD TEMP[28].x, TEMP[19].xxxx, IMM[6].xxxx
 49:   SIN TEMP[29].x, TEMP[28].xxxx
 50:   MUL TEMP[30].x, TEMP[29].xxxx, IMM[6].zzzz
 51:   FRC TEMP[31].x, TEMP[30].xxxx
 52:   ADD TEMP[32].x, TEMP[19].xxxx, IMM[7].xxxx
 53:   SIN TEMP[33].x, TEMP[32].xxxx
 54:   MUL TEMP[34].x, TEMP[33].xxxx, IMM[6].zzzz
 55:   FRC TEMP[35].x, TEMP[34].xxxx
 56:   LRP TEMP[36].x, TEMP[17].xxxx, TEMP[35].xxxx, TEMP[31].xxxx
 57:   LRP TEMP[37].x, TEMP[17].yyyy, TEMP[36].xxxx, TEMP[27].xxxx
 58:   ADD TEMP[38].x, TEMP[19].xxxx, IMM[5].wwww
 59:   SIN TEMP[39].x, TEMP[38].xxxx
 60:   MUL TEMP[40].x, TEMP[39].xxxx, IMM[6].zzzz
 61:   FRC TEMP[41].x, TEMP[40].xxxx
 62:   ADD TEMP[42].x, TEMP[19].xxxx, IMM[7].yyyy
 63:   SIN TEMP[43].x, TEMP[42].xxxx
 64:   MUL TEMP[44].x, TEMP[43].xxxx, IMM[6].zzzz
 65:   FRC TEMP[45].x, TEMP[44].xxxx
 66:   LRP TEMP[46].x, TEMP[17].xxxx, TEMP[45].xxxx, TEMP[41].xxxx
 67:   ADD TEMP[47].x, TEMP[19].xxxx, IMM[7].zzzz
 68:   SIN TEMP[48].x, TEMP[47].xxxx
 69:   MUL TEMP[49].x, TEMP[48].xxxx, IMM[6].zzzz
 70:   FRC TEMP[50].x, TEMP[49].xxxx
 71:   ADD TEMP[51].x, TEMP[19].xxxx, IMM[7].wwww
 72:   SIN TEMP[52].x, TEMP[51].xxxx
 73:   MUL TEMP[53].x, TEMP[52].xxxx, IMM[6].zzzz
 74:   FRC TEMP[54].x, TEMP[53].xxxx
 75:   LRP TEMP[55].x, TEMP[17].xxxx, TEMP[54].xxxx, TEMP[50].xxxx
 76:   LRP TEMP[56].x, TEMP[17].yyyy, TEMP[55].xxxx, TEMP[46].xxxx
 77:   LRP TEMP[57].x, TEMP[17].zzzz, TEMP[56].xxxx, TEMP[37].xxxx
 78:   MUL TEMP[58].x, IMM[6].yyyy, TEMP[57].xxxx
 79:   MUL TEMP[59].xyz, IMM[10].xyzz, TEMP[11].xxxx
 80:   MAD TEMP[60].xyz, IMM[9].xyzz, TEMP[11].yyyy, TEMP[59].xyzz
 81:   MAD TEMP[61].xyz, IMM[8].xyzz, TEMP[11].zzzz, TEMP[60].xyzz
 82:   MUL TEMP[11].xyz, TEMP[61].xyzz, IMM[8].wwww
 83:   FLR TEMP[62].xyz, TEMP[11].xyzz
 84:   FRC TEMP[63].xyz, TEMP[11].xyzz
 85:   MUL TEMP[64].xyz, TEMP[63].xyzz, TEMP[63].xyzz
 86:   MUL TEMP[65].xyz, IMM[0].xxxx, TEMP[63].xyzz
 87:   ADD TEMP[66].xyz, IMM[5].zzzz, -TEMP[65].xyzz
 88:   MUL TEMP[67].xyz, TEMP[64].xyzz, TEMP[66].xyzz
 89:   MAD TEMP[68].x, TEMP[62].yyyy, IMM[6].xxxx, TEMP[62].xxxx
 90:   MAD TEMP[69].x, IMM[5].wwww, TEMP[62].zzzz, TEMP[68].xxxx
 91:   SIN TEMP[70].x, TEMP[69].xxxx
 92:   MUL TEMP[71].x, TEMP[70].xxxx, IMM[6].zzzz
 93:   FRC TEMP[72].x, TEMP[71].xxxx
 94:   ADD TEMP[73].x, TEMP[69].xxxx, IMM[6].wwww
 95:   SIN TEMP[74].x, TEMP[73].xxxx
 96:   MUL TEMP[75].x, TEMP[74].xxxx, IMM[6].zzzz
 97:   FRC TEMP[76].x, TEMP[75].xxxx
 98:   LRP TEMP[77].x, TEMP[67].xxxx, TEMP[76].xxxx, TEMP[72].xxxx
 99:   ADD TEMP[78].x, TEMP[69].xxxx, IMM[6].xxxx
100:   SIN TEMP[79].x, TEMP[78].xxxx
101:   MUL TEMP[80].x, TEMP[79].xxxx, IMM[6].zzzz
102:   FRC TEMP[81].x, TEMP[80].xxxx
103:   ADD TEMP[82].x, TEMP[69].xxxx, IMM[7].xxxx
104:   SIN TEMP[83].x, TEMP[82].xxxx
105:   MUL TEMP[84].x, TEMP[83].xxxx, IMM[6].zzzz
106:   FRC TEMP[85].x, TEMP[84].xxxx
107:   LRP TEMP[86].x, TEMP[67].xxxx, TEMP[85].xxxx, TEMP[81].xxxx
108:   LRP TEMP[87].x, TEMP[67].yyyy, TEMP[86].xxxx, TEMP[77].xxxx
109:   ADD TEMP[88].x, TEMP[69].xxxx, IMM[5].wwww
110:   SIN TEMP[89].x, TEMP[88].xxxx
111:   MUL TEMP[90].x, TEMP[89].xxxx, IMM[6].zzzz
112:   FRC TEMP[91].x, TEMP[90].xxxx
113:   ADD TEMP[92].x, TEMP[69].xxxx, IMM[7].yyyy
114:   SIN TEMP[93].x, TEMP[92].xxxx
115:   MUL TEMP[94].x, TEMP[93].xxxx, IMM[6].zzzz
116:   FRC TEMP[95].x, TEMP[94].xxxx
117:   LRP TEMP[96].x, TEMP[67].xxxx, TEMP[95].xxxx, TEMP[91].xxxx
118:   ADD TEMP[97].x, TEMP[69].xxxx, IMM[7].zzzz
119:   SIN TEMP[98].x, TEMP[97].xxxx
120:   MUL TEMP[99].x, TEMP[98].xxxx, IMM[6].zzzz
121:   FRC TEMP[100].x, TEMP[99].xxxx
122:   ADD TEMP[101].x, TEMP[69].xxxx, IMM[7].wwww
123:   SIN TEMP[102].x, TEMP[101].xxxx
124:   MUL TEMP[103].x, TEMP[102].xxxx, IMM[6].zzzz
125:   FRC TEMP[104].x, TEMP[103].xxxx
126:   LRP TEMP[105].x, TEMP[67].xxxx, TEMP[104].xxxx, TEMP[100].xxxx
127:   LRP TEMP[106].x, TEMP[67].yyyy, TEMP[105].xxxx, TEMP[96].xxxx
128:   LRP TEMP[107].x, TEMP[67].zzzz, TEMP[106].xxxx, TEMP[87].xxxx
129:   MAD TEMP[58].x, IMM[9].wwww, TEMP[107].xxxx, TEMP[58].xxxx
130:   MUL TEMP[108].xyz, IMM[10].xyzz, TEMP[11].xxxx
131:   MAD TEMP[109].xyz, IMM[9].xyzz, TEMP[11].yyyy, TEMP[108].xyzz
132:   MAD TEMP[110].xyz, IMM[8].xyzz, TEMP[11].zzzz, TEMP[109].xyzz
133:   MUL TEMP[11].xyz, TEMP[110].xyzz, IMM[10].wwww
134:   FLR TEMP[111].xyz, TEMP[11].xyzz
135:   FRC TEMP[112].xyz, TEMP[11].xyzz
136:   MUL TEMP[113].xyz, TEMP[112].xyzz, TEMP[112].xyzz
137:   MUL TEMP[114].xyz, IMM[0].xxxx, TEMP[112].xyzz
138:   ADD TEMP[115].xyz, IMM[5].zzzz, -TEMP[114].xyzz
139:   MUL TEMP[116].xyz, TEMP[113].xyzz, TEMP[115].xyzz
140:   MAD TEMP[117].x, TEMP[111].yyyy, IMM[6].xxxx, TEMP[111].xxxx
141:   MAD TEMP[118].x, IMM[5].wwww, TEMP[111].zzzz, TEMP[117].xxxx
142:   SIN TEMP[119].x, TEMP[118].xxxx
143:   MUL TEMP[120].x, TEMP[119].xxxx, IMM[6].zzzz
144:   FRC TEMP[121].x, TEMP[120].xxxx
145:   ADD TEMP[122].x, TEMP[118].xxxx, IMM[6].wwww
146:   SIN TEMP[123].x, TEMP[122].xxxx
147:   MUL TEMP[124].x, TEMP[123].xxxx, IMM[6].zzzz
148:   FRC TEMP[125].x, TEMP[124].xxxx
149:   LRP TEMP[126].x, TEMP[116].xxxx, TEMP[125].xxxx, TEMP[121].xxxx
150:   ADD TEMP[127].x, TEMP[118].xxxx, IMM[6].xxxx
151:   SIN TEMP[128].x, TEMP[127].xxxx
152:   MUL TEMP[129].x, TEMP[128].xxxx, IMM[6].zzzz
153:   FRC TEMP[130].x, TEMP[129].xxxx
154:   ADD TEMP[131].x, TEMP[118].xxxx, IMM[7].xxxx
155:   SIN TEMP[132].x, TEMP[131].xxxx
156:   MUL TEMP[133].x, TEMP[132].xxxx, IMM[6].zzzz
157:   FRC TEMP[134].x, TEMP[133].xxxx
158:   LRP TEMP[135].x, TEMP[116].xxxx, TEMP[134].xxxx, TEMP[130].xxxx
159:   LRP TEMP[136].x, TEMP[116].yyyy, TEMP[135].xxxx, TEMP[126].xxxx
160:   ADD TEMP[137].x, TEMP[118].xxxx, IMM[5].wwww
161:   SIN TEMP[138].x, TEMP[137].xxxx
162:   MUL TEMP[139].x, TEMP[138].xxxx, IMM[6].zzzz
163:   FRC TEMP[140].x, TEMP[139].xxxx
164:   ADD TEMP[141].x, TEMP[118].xxxx, IMM[7].yyyy
165:   SIN TEMP[142].x, TEMP[141].xxxx
166:   MUL TEMP[143].x, TEMP[142].xxxx, IMM[6].zzzz
167:   FRC TEMP[144].x, TEMP[143].xxxx
168:   LRP TEMP[145].x, TEMP[116].xxxx, TEMP[144].xxxx, TEMP[140].xxxx
169:   ADD TEMP[146].x, TEMP[118].xxxx, IMM[7].zzzz
170:   SIN TEMP[147].x, TEMP[146].xxxx
171:   MUL TEMP[148].x, TEMP[147].xxxx, IMM[6].zzzz
172:   FRC TEMP[149].x, TEMP[148].xxxx
173:   ADD TEMP[150].x, TEMP[118].xxxx, IMM[7].wwww
174:   SIN TEMP[151].x, TEMP[150].xxxx
175:   MUL TEMP[152].x, TEMP[151].xxxx, IMM[6].zzzz
176:   FRC TEMP[153].x, TEMP[152].xxxx
177:   LRP TEMP[154].x, TEMP[116].xxxx, TEMP[153].xxxx, TEMP[149].xxxx
178:   LRP TEMP[155].x, TEMP[116].yyyy, TEMP[154].xxxx, TEMP[145].xxxx
179:   LRP TEMP[156].x, TEMP[116].zzzz, TEMP[155].xxxx, TEMP[136].xxxx
180:   MAD TEMP[58].x, IMM[11].xxxx, TEMP[156].xxxx, TEMP[58].xxxx
181:   MUL TEMP[157].xyz, IMM[10].xyzz, TEMP[11].xxxx
182:   MAD TEMP[158].xyz, IMM[9].xyzz, TEMP[11].yyyy, TEMP[157].xyzz
183:   MAD TEMP[159].xyz, IMM[8].xyzz, TEMP[11].zzzz, TEMP[158].xyzz
184:   MUL TEMP[11].xyz, TEMP[159].xyzz, IMM[11].yyyy
185:   FLR TEMP[160].xyz, TEMP[11].xyzz
186:   FRC TEMP[161].xyz, TEMP[11].xyzz
187:   MUL TEMP[162].xyz, TEMP[161].xyzz, TEMP[161].xyzz
188:   MUL TEMP[163].xyz, IMM[0].xxxx, TEMP[161].xyzz
189:   ADD TEMP[164].xyz, IMM[5].zzzz, -TEMP[163].xyzz
190:   MUL TEMP[165].xyz, TEMP[162].xyzz, TEMP[164].xyzz
191:   MAD TEMP[166].x, TEMP[160].yyyy, IMM[6].xxxx, TEMP[160].xxxx
192:   MAD TEMP[167].x, IMM[5].wwww, TEMP[160].zzzz, TEMP[166].xxxx
193:   SIN TEMP[168].x, TEMP[167].xxxx
194:   MUL TEMP[169].x, TEMP[168].xxxx, IMM[6].zzzz
195:   FRC TEMP[170].x, TEMP[169].xxxx
196:   ADD TEMP[171].x, TEMP[167].xxxx, IMM[6].wwww
197:   SIN TEMP[172].x, TEMP[171].xxxx
198:   MUL TEMP[173].x, TEMP[172].xxxx, IMM[6].zzzz
199:   FRC TEMP[174].x, TEMP[173].xxxx
200:   LRP TEMP[175].x, TEMP[165].xxxx, TEMP[174].xxxx, TEMP[170].xxxx
201:   ADD TEMP[176].x, TEMP[167].xxxx, IMM[6].xxxx
202:   SIN TEMP[177].x, TEMP[176].xxxx
203:   MUL TEMP[178].x, TEMP[177].xxxx, IMM[6].zzzz
204:   FRC TEMP[179].x, TEMP[178].xxxx
205:   ADD TEMP[180].x, TEMP[167].xxxx, IMM[7].xxxx
206:   SIN TEMP[181].x, TEMP[180].xxxx
207:   MUL TEMP[182].x, TEMP[181].xxxx, IMM[6].zzzz
208:   FRC TEMP[183].x, TEMP[182].xxxx
209:   LRP TEMP[184].x, TEMP[165].xxxx, TEMP[183].xxxx, TEMP[179].xxxx
210:   LRP TEMP[185].x, TEMP[165].yyyy, TEMP[184].xxxx, TEMP[175].xxxx
211:   ADD TEMP[186].x, TEMP[167].xxxx, IMM[5].wwww
212:   SIN TEMP[187].x, TEMP[186].xxxx
213:   MUL TEMP[188].x, TEMP[187].xxxx, IMM[6].zzzz
214:   FRC TEMP[189].x, TEMP[188].xxxx
215:   ADD TEMP[190].x, TEMP[167].xxxx, IMM[7].yyyy
216:   SIN TEMP[191].x, TEMP[190].xxxx
217:   MUL TEMP[192].x, TEMP[191].xxxx, IMM[6].zzzz
218:   FRC TEMP[193].x, TEMP[192].xxxx
219:   LRP TEMP[194].x, TEMP[165].xxxx, TEMP[193].xxxx, TEMP[189].xxxx
220:   ADD TEMP[195].x, TEMP[167].xxxx, IMM[7].zzzz
221:   SIN TEMP[196].x, TEMP[195].xxxx
222:   MUL TEMP[197].x, TEMP[196].xxxx, IMM[6].zzzz
223:   FRC TEMP[198].x, TEMP[197].xxxx
224:   ADD TEMP[199].x, TEMP[167].xxxx, IMM[7].wwww
225:   SIN TEMP[200].x, TEMP[199].xxxx
226:   MUL TEMP[201].x, TEMP[200].xxxx, IMM[6].zzzz
227:   FRC TEMP[202].x, TEMP[201].xxxx
228:   LRP TEMP[203].x, TEMP[165].xxxx, TEMP[202].xxxx, TEMP[198].xxxx
229:   LRP TEMP[204].x, TEMP[165].yyyy, TEMP[203].xxxx, TEMP[194].xxxx
230:   LRP TEMP[205].x, TEMP[165].zzzz, TEMP[204].xxxx, TEMP[185].xxxx
231:   MAD TEMP[58].x, IMM[11].zzzz, TEMP[205].xxxx, TEMP[58].xxxx
232:   MUL TEMP[206].xyz, IMM[10].xyzz, TEMP[11].xxxx
233:   MAD TEMP[207].xyz, IMM[9].xyzz, TEMP[11].yyyy, TEMP[206].xyzz
234:   MAD TEMP[208].xyz, IMM[8].xyzz, TEMP[11].zzzz, TEMP[207].xyzz
235:   MUL TEMP[11].xyz, TEMP[208].xyzz, IMM[8].wwww
236:   FLR TEMP[209].xyz, TEMP[11].xyzz
237:   FRC TEMP[210].xyz, TEMP[11].xyzz
238:   MUL TEMP[211].xyz, TEMP[210].xyzz, TEMP[210].xyzz
239:   MUL TEMP[212].xyz, IMM[0].xxxx, TEMP[210].xyzz
240:   ADD TEMP[213].xyz, IMM[5].zzzz, -TEMP[212].xyzz
241:   MUL TEMP[214].xyz, TEMP[211].xyzz, TEMP[213].xyzz
242:   MAD TEMP[215].x, TEMP[209].yyyy, IMM[6].xxxx, TEMP[209].xxxx
243:   MAD TEMP[216].x, IMM[5].wwww, TEMP[209].zzzz, TEMP[215].xxxx
244:   SIN TEMP[217].x, TEMP[216].xxxx
245:   MUL TEMP[218].x, TEMP[217].xxxx, IMM[6].zzzz
246:   FRC TEMP[219].x, TEMP[218].xxxx
247:   ADD TEMP[220].x, TEMP[216].xxxx, IMM[6].wwww
248:   SIN TEMP[221].x, TEMP[220].xxxx
249:   MUL TEMP[222].x, TEMP[221].xxxx, IMM[6].zzzz
250:   FRC TEMP[223].x, TEMP[222].xxxx
251:   LRP TEMP[224].x, TEMP[214].xxxx, TEMP[223].xxxx, TEMP[219].xxxx
252:   ADD TEMP[225].x, TEMP[216].xxxx, IMM[6].xxxx
253:   SIN TEMP[226].x, TEMP[225].xxxx
254:   MUL TEMP[227].x, TEMP[226].xxxx, IMM[6].zzzz
255:   FRC TEMP[228].x, TEMP[227].xxxx
256:   ADD TEMP[229].x, TEMP[216].xxxx, IMM[7].xxxx
257:   SIN TEMP[230].x, TEMP[229].xxxx
258:   MUL TEMP[231].x, TEMP[230].xxxx, IMM[6].zzzz
259:   FRC TEMP[232].x, TEMP[231].xxxx
260:   LRP TEMP[233].x, TEMP[214].xxxx, TEMP[232].xxxx, TEMP[228].xxxx
261:   LRP TEMP[234].x, TEMP[214].yyyy, TEMP[233].xxxx, TEMP[224].xxxx
262:   ADD TEMP[235].x, TEMP[216].xxxx, IMM[5].wwww
263:   SIN TEMP[236].x, TEMP[235].xxxx
264:   MUL TEMP[237].x, TEMP[236].xxxx, IMM[6].zzzz
265:   FRC TEMP[238].x, TEMP[237].xxxx
266:   ADD TEMP[239].x, TEMP[216].xxxx, IMM[7].yyyy
267:   SIN TEMP[240].x, TEMP[239].xxxx
268:   MUL TEMP[241].x, TEMP[240].xxxx, IMM[6].zzzz
269:   FRC TEMP[242].x, TEMP[241].xxxx
270:   LRP TEMP[243].x, TEMP[214].xxxx, TEMP[242].xxxx, TEMP[238].xxxx
271:   ADD TEMP[244].x, TEMP[216].xxxx, IMM[7].zzzz
272:   SIN TEMP[245].x, TEMP[244].xxxx
273:   MUL TEMP[246].x, TEMP[245].xxxx, IMM[6].zzzz
274:   FRC TEMP[247].x, TEMP[246].xxxx
275:   ADD TEMP[248].x, TEMP[216].xxxx, IMM[7].wwww
276:   SIN TEMP[249].x, TEMP[248].xxxx
277:   MUL TEMP[250].x, TEMP[249].xxxx, IMM[6].zzzz
278:   FRC TEMP[251].x, TEMP[250].xxxx
279:   LRP TEMP[252].x, TEMP[214].xxxx, TEMP[251].xxxx, TEMP[247].xxxx
280:   LRP TEMP[253].x, TEMP[214].yyyy, TEMP[252].xxxx, TEMP[243].xxxx
281:   LRP TEMP[254].x, TEMP[214].zzzz, TEMP[253].xxxx, TEMP[234].xxxx
282:   ABS TEMP[255].x, TEMP[254].xxxx
283:   MAD TEMP[58].x, IMM[11].wwww, TEMP[255].xxxx, TEMP[58].xxxx
284:   MUL TEMP[256].x, TEMP[58].xxxx, IMM[12].xxxx
285:   MAD TEMP[9].x, TEMP[256].xxxx, IMM[12].yyyy, TEMP[9].xxxx
286:   FSLT TEMP[257].x, TEMP[9].xxxx, IMM[12].zzzz
287:   UIF TEMP[257].xxxx :2
288:     MOV TEMP[3].x, IMM[3].yyyy
289:     MOV TEMP[4].x, TEMP[256].xxxx
290:   ENDIF
291:   MUL TEMP[258].xyz, TEMP[1].xyzz, TEMP[9].xxxx
292:   MAD TEMP[2].xyz, TEMP[258].xyzz, IMM[12].wwww, TEMP[2].xyzz
293:   UADD TEMP[5].x, TEMP[5].xxxx, IMM[4].zzzz
294: ENDLOOP :2
295: MOV TEMP[1], IMM[13].xxxy
296: UIF TEMP[3].xxxx :2
297:   MAD TEMP[3].x, TEMP[4].xxxx, IMM[13].zzzz, IMM[13].wwww
298:   MIN TEMP[3].x, TEMP[3].xxxx, IMM[14].xxxx
299:   MOV_SAT TEMP[3].x, TEMP[3].xxxx
300:   MUL TEMP[4].x, TEMP[3].xxxx, IMM[5].zzzz
301:   FRC TEMP[4].x, TEMP[4].xxxx
302:   FSLT TEMP[5].x, TEMP[3].xxxx, IMM[14].yyyy
303:   UIF TEMP[5].xxxx :2
304:     LRP TEMP[5], TEMP[4].xxxx, IMM[15].xyzx, IMM[14].zzww
305:   ELSE :2
306:     FSLT TEMP[6].x, TEMP[3].xxxx, IMM[15].wwww
307:     UIF TEMP[6].xxxx :2
308:       LRP TEMP[5], TEMP[4].xxxx, IMM[15].xzzx, IMM[15].xyzx
309:     ELSE :2
310:       LRP TEMP[5], TEMP[4].xxxx, IMM[16].xxxy, IMM[15].xzzx
311:     ENDIF
312:   ENDIF
313:   MOV TEMP[4].w, TEMP[5].wwww
314:   DP3 TEMP[6].x, TEMP[2].xyzz, TEMP[2].xyzz
315:   RSQ TEMP[7].x, TEMP[6].xxxx
316:   MUL TEMP[7].x, TEMP[7].xxxx, TEMP[6].xxxx
317:   CMP TEMP[7].x, -TEMP[6].xxxx, TEMP[7].xxxx, IMM[0].zzzz
318:   ADD TEMP[6].x, TEMP[7].xxxx, IMM[2].zzzz
319:   MUL TEMP[7].xyz, TEMP[2].xyzz, IMM[2].wwww
320:   MAD TEMP[7].xyz, IMM[5].xyyy, CONST[1].xxxx, TEMP[7].xyzz
321:   FLR TEMP[8].xyz, TEMP[7].xyzz
322:   FRC TEMP[9].xyz, TEMP[7].xyzz
323:   MUL TEMP[10].xyz, TEMP[9].xyzz, TEMP[9].xyzz
324:   MUL TEMP[9].xyz, IMM[0].xxxx, TEMP[9].xyzz
325:   ADD TEMP[9].xyz, IMM[5].zzzz, -TEMP[9].xyzz
326:   MUL TEMP[9].xyz, TEMP[10].xyzz, TEMP[9].xyzz
327:   MAD TEMP[10].x, TEMP[8].yyyy, IMM[6].xxxx, TEMP[8].xxxx
328:   MAD TEMP[8].x, IMM[5].wwww, TEMP[8].zzzz, TEMP[10].xxxx
329:   SIN TEMP[10].x, TEMP[8].xxxx
330:   MUL TEMP[10].x, TEMP[10].xxxx, IMM[6].zzzz
331:   FRC TEMP[10].x, TEMP[10].xxxx
332:   ADD TEMP[11].x, TEMP[8].xxxx, IMM[6].wwww
333:   SIN TEMP[11].x, TEMP[11].xxxx
334:   MUL TEMP[11].x, TEMP[11].xxxx, IMM[6].zzzz
335:   FRC TEMP[11].x, TEMP[11].xxxx
336:   LRP TEMP[10].x, TEMP[9].xxxx, TEMP[11].xxxx, TEMP[10].xxxx
337:   ADD TEMP[11].x, TEMP[8].xxxx, IMM[6].xxxx
338:   SIN TEMP[11].x, TEMP[11].xxxx
339:   MUL TEMP[11].x, TEMP[11].xxxx, IMM[6].zzzz
340:   FRC TEMP[11].x, TEMP[11].xxxx
341:   ADD TEMP[12].x, TEMP[8].xxxx, IMM[7].xxxx
342:   SIN TEMP[12].x, TEMP[12].xxxx
343:   MUL TEMP[12].x, TEMP[12].xxxx, IMM[6].zzzz
344:   FRC TEMP[12].x, TEMP[12].xxxx
345:   LRP TEMP[11].x, TEMP[9].xxxx, TEMP[12].xxxx, TEMP[11].xxxx
346:   LRP TEMP[10].x, TEMP[9].yyyy, TEMP[11].xxxx, TEMP[10].xxxx
347:   ADD TEMP[11].x, TEMP[8].xxxx, IMM[5].wwww
348:   SIN TEMP[11].x, TEMP[11].xxxx
349:   MUL TEMP[11].x, TEMP[11].xxxx, IMM[6].zzzz
350:   FRC TEMP[11].x, TEMP[11].xxxx
351:   ADD TEMP[12].x, TEMP[8].xxxx, IMM[7].yyyy
352:   SIN TEMP[12].x, TEMP[12].xxxx
353:   MUL TEMP[12].x, TEMP[12].xxxx, IMM[6].zzzz
354:   FRC TEMP[12].x, TEMP[12].xxxx
355:   LRP TEMP[11].x, TEMP[9].xxxx, TEMP[12].xxxx, TEMP[11].xxxx
356:   ADD TEMP[12].x, TEMP[8].xxxx, IMM[7].zzzz
357:   SIN TEMP[12].x, TEMP[12].xxxx
358:   MUL TEMP[12].x, TEMP[12].xxxx, IMM[6].zzzz
359:   FRC TEMP[12].x, TEMP[12].xxxx
360:   ADD TEMP[8].x, TEMP[8].xxxx, IMM[7].wwww
361:   SIN TEMP[8].x, TEMP[8].xxxx
362:   MUL TEMP[8].x, TEMP[8].xxxx, IMM[6].zzzz
363:   FRC TEMP[8].x, TEMP[8].xxxx
364:   LRP TEMP[8].x, TEMP[9].xxxx, TEMP[8].xxxx, TEMP[12].xxxx
365:   LRP TEMP[8].x, TEMP[9].yyyy, TEMP[8].xxxx, TEMP[11].xxxx
366:   LRP TEMP[8].x, TEMP[9].zzzz, TEMP[8].xxxx, TEMP[10].xxxx
367:   MUL TEMP[8].x, IMM[6].yyyy, TEMP[8].xxxx
368:   MUL TEMP[9].xyz, IMM[10].xyzz, TEMP[7].xxxx
369:   MAD TEMP[9].xyz, IMM[9].xyzz, TEMP[7].yyyy, TEMP[9].xyzz
370:   MAD TEMP[9].xyz, IMM[8].xyzz, TEMP[7].zzzz, TEMP[9].xyzz
371:   MUL TEMP[7].xyz, TEMP[9].xyzz, IMM[8].wwww
372:   FLR TEMP[9].xyz, TEMP[7].xyzz
373:   FRC TEMP[10].xyz, TEMP[7].xyzz
374:   MUL TEMP[11].xyz, TEMP[10].xyzz, TEMP[10].xyzz
375:   MUL TEMP[10].xyz, IMM[0].xxxx, TEMP[10].xyzz
376:   ADD TEMP[10].xyz, IMM[5].zzzz, -TEMP[10].xyzz
377:   MUL TEMP[10].xyz, TEMP[11].xyzz, TEMP[10].xyzz
378:   MAD TEMP[11].x, TEMP[9].yyyy, IMM[6].xxxx, TEMP[9].xxxx
379:   MAD TEMP[9].x, IMM[5].wwww, TEMP[9].zzzz, TEMP[11].xxxx
380:   SIN TEMP[11].x, TEMP[9].xxxx
381:   MUL TEMP[11].x, TEMP[11].xxxx, IMM[6].zzzz
382:   FRC TEMP[11].x, TEMP[11].xxxx
383:   ADD TEMP[12].x, TEMP[9].xxxx, IMM[6].wwww
384:   SIN TEMP[12].x, TEMP[12].xxxx
385:   MUL TEMP[12].x, TEMP[12].xxxx, IMM[6].zzzz
386:   FRC TEMP[12].x, TEMP[12].xxxx
387:   LRP TEMP[11].x, TEMP[10].xxxx, TEMP[12].xxxx, TEMP[11].xxxx
388:   ADD TEMP[12].x, TEMP[9].xxxx, IMM[6].xxxx
389:   SIN TEMP[12].x, TEMP[12].xxxx
390:   MUL TEMP[12].x, TEMP[12].xxxx, IMM[6].zzzz
391:   FRC TEMP[12].x, TEMP[12].xxxx
392:   ADD TEMP[13].x, TEMP[9].xxxx, IMM[7].xxxx
393:   SIN TEMP[13].x, TEMP[13].xxxx
394:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
395:   FRC TEMP[13].x, TEMP[13].xxxx
396:   LRP TEMP[12].x, TEMP[10].xxxx, TEMP[13].xxxx, TEMP[12].xxxx
397:   LRP TEMP[11].x, TEMP[10].yyyy, TEMP[12].xxxx, TEMP[11].xxxx
398:   ADD TEMP[12].x, TEMP[9].xxxx, IMM[5].wwww
399:   SIN TEMP[12].x, TEMP[12].xxxx
400:   MUL TEMP[12].x, TEMP[12].xxxx, IMM[6].zzzz
401:   FRC TEMP[12].x, TEMP[12].xxxx
402:   ADD TEMP[13].x, TEMP[9].xxxx, IMM[7].yyyy
403:   SIN TEMP[13].x, TEMP[13].xxxx
404:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
405:   FRC TEMP[13].x, TEMP[13].xxxx
406:   LRP TEMP[12].x, TEMP[10].xxxx, TEMP[13].xxxx, TEMP[12].xxxx
407:   ADD TEMP[13].x, TEMP[9].xxxx, IMM[7].zzzz
408:   SIN TEMP[13].x, TEMP[13].xxxx
409:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
410:   FRC TEMP[13].x, TEMP[13].xxxx
411:   ADD TEMP[9].x, TEMP[9].xxxx, IMM[7].wwww
412:   SIN TEMP[9].x, TEMP[9].xxxx
413:   MUL TEMP[9].x, TEMP[9].xxxx, IMM[6].zzzz
414:   FRC TEMP[9].x, TEMP[9].xxxx
415:   LRP TEMP[9].x, TEMP[10].xxxx, TEMP[9].xxxx, TEMP[13].xxxx
416:   LRP TEMP[9].x, TEMP[10].yyyy, TEMP[9].xxxx, TEMP[12].xxxx
417:   LRP TEMP[9].x, TEMP[10].zzzz, TEMP[9].xxxx, TEMP[11].xxxx
418:   MAD TEMP[8].x, IMM[9].wwww, TEMP[9].xxxx, TEMP[8].xxxx
419:   MUL TEMP[9].xyz, IMM[10].xyzz, TEMP[7].xxxx
420:   MAD TEMP[9].xyz, IMM[9].xyzz, TEMP[7].yyyy, TEMP[9].xyzz
421:   MAD TEMP[9].xyz, IMM[8].xyzz, TEMP[7].zzzz, TEMP[9].xyzz
422:   MUL TEMP[7].xyz, TEMP[9].xyzz, IMM[10].wwww
423:   FLR TEMP[9].xyz, TEMP[7].xyzz
424:   FRC TEMP[10].xyz, TEMP[7].xyzz
425:   MUL TEMP[11].xyz, TEMP[10].xyzz, TEMP[10].xyzz
426:   MUL TEMP[10].xyz, IMM[0].xxxx, TEMP[10].xyzz
427:   ADD TEMP[10].xyz, IMM[5].zzzz, -TEMP[10].xyzz
428:   MUL TEMP[10].xyz, TEMP[11].xyzz, TEMP[10].xyzz
429:   MAD TEMP[11].x, TEMP[9].yyyy, IMM[6].xxxx, TEMP[9].xxxx
430:   MAD TEMP[9].x, IMM[5].wwww, TEMP[9].zzzz, TEMP[11].xxxx
431:   SIN TEMP[11].x, TEMP[9].xxxx
432:   MUL TEMP[11].x, TEMP[11].xxxx, IMM[6].zzzz
433:   FRC TEMP[11].x, TEMP[11].xxxx
434:   ADD TEMP[12].x, TEMP[9].xxxx, IMM[6].wwww
435:   SIN TEMP[12].x, TEMP[12].xxxx
436:   MUL TEMP[12].x, TEMP[12].xxxx, IMM[6].zzzz
437:   FRC TEMP[12].x, TEMP[12].xxxx
438:   LRP TEMP[11].x, TEMP[10].xxxx, TEMP[12].xxxx, TEMP[11].xxxx
439:   ADD TEMP[12].x, TEMP[9].xxxx, IMM[6].xxxx
440:   SIN TEMP[12].x, TEMP[12].xxxx
441:   MUL TEMP[12].x, TEMP[12].xxxx, IMM[6].zzzz
442:   FRC TEMP[12].x, TEMP[12].xxxx
443:   ADD TEMP[13].x, TEMP[9].xxxx, IMM[7].xxxx
444:   SIN TEMP[13].x, TEMP[13].xxxx
445:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
446:   FRC TEMP[13].x, TEMP[13].xxxx
447:   LRP TEMP[12].x, TEMP[10].xxxx, TEMP[13].xxxx, TEMP[12].xxxx
448:   LRP TEMP[11].x, TEMP[10].yyyy, TEMP[12].xxxx, TEMP[11].xxxx
449:   ADD TEMP[12].x, TEMP[9].xxxx, IMM[5].wwww
450:   SIN TEMP[12].x, TEMP[12].xxxx
451:   MUL TEMP[12].x, TEMP[12].xxxx, IMM[6].zzzz
452:   FRC TEMP[12].x, TEMP[12].xxxx
453:   ADD TEMP[13].x, TEMP[9].xxxx, IMM[7].yyyy
454:   SIN TEMP[13].x, TEMP[13].xxxx
455:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
456:   FRC TEMP[13].x, TEMP[13].xxxx
457:   LRP TEMP[12].x, TEMP[10].xxxx, TEMP[13].xxxx, TEMP[12].xxxx
458:   ADD TEMP[13].x, TEMP[9].xxxx, IMM[7].zzzz
459:   SIN TEMP[13].x, TEMP[13].xxxx
460:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
461:   FRC TEMP[13].x, TEMP[13].xxxx
462:   ADD TEMP[9].x, TEMP[9].xxxx, IMM[7].wwww
463:   SIN TEMP[9].x, TEMP[9].xxxx
464:   MUL TEMP[9].x, TEMP[9].xxxx, IMM[6].zzzz
465:   FRC TEMP[9].x, TEMP[9].xxxx
466:   LRP TEMP[9].x, TEMP[10].xxxx, TEMP[9].xxxx, TEMP[13].xxxx
467:   LRP TEMP[9].x, TEMP[10].yyyy, TEMP[9].xxxx, TEMP[12].xxxx
468:   LRP TEMP[9].x, TEMP[10].zzzz, TEMP[9].xxxx, TEMP[11].xxxx
469:   MAD TEMP[8].x, IMM[11].xxxx, TEMP[9].xxxx, TEMP[8].xxxx
470:   MUL TEMP[9].xyz, IMM[10].xyzz, TEMP[7].xxxx
471:   MAD TEMP[9].xyz, IMM[9].xyzz, TEMP[7].yyyy, TEMP[9].xyzz
472:   MAD TEMP[9].xyz, IMM[8].xyzz, TEMP[7].zzzz, TEMP[9].xyzz
473:   MUL TEMP[7].xyz, TEMP[9].xyzz, IMM[11].yyyy
474:   FLR TEMP[9].xyz, TEMP[7].xyzz
475:   FRC TEMP[10].xyz, TEMP[7].xyzz
476:   MUL TEMP[11].xyz, TEMP[10].xyzz, TEMP[10].xyzz
477:   MUL TEMP[10].xyz, IMM[0].xxxx, TEMP[10].xyzz
478:   ADD TEMP[10].xyz, IMM[5].zzzz, -TEMP[10].xyzz
479:   MUL TEMP[10].xyz, TEMP[11].xyzz, TEMP[10].xyzz
480:   MAD TEMP[11].x, TEMP[9].yyyy, IMM[6].xxxx, TEMP[9].xxxx
481:   MAD TEMP[9].x, IMM[5].wwww, TEMP[9].zzzz, TEMP[11].xxxx
482:   SIN TEMP[11].x, TEMP[9].xxxx
483:   MUL TEMP[11].x, TEMP[11].xxxx, IMM[6].zzzz
484:   FRC TEMP[11].x, TEMP[11].xxxx
485:   ADD TEMP[12].x, TEMP[9].xxxx, IMM[6].wwww
486:   SIN TEMP[12].x, TEMP[12].xxxx
487:   MUL TEMP[12].x, TEMP[12].xxxx, IMM[6].zzzz
488:   FRC TEMP[12].x, TEMP[12].xxxx
489:   LRP TEMP[11].x, TEMP[10].xxxx, TEMP[12].xxxx, TEMP[11].xxxx
490:   ADD TEMP[12].x, TEMP[9].xxxx, IMM[6].xxxx
491:   SIN TEMP[12].x, TEMP[12].xxxx
492:   MUL TEMP[12].x, TEMP[12].xxxx, IMM[6].zzzz
493:   FRC TEMP[12].x, TEMP[12].xxxx
494:   ADD TEMP[13].x, TEMP[9].xxxx, IMM[7].xxxx
495:   SIN TEMP[13].x, TEMP[13].xxxx
496:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
497:   FRC TEMP[13].x, TEMP[13].xxxx
498:   LRP TEMP[12].x, TEMP[10].xxxx, TEMP[13].xxxx, TEMP[12].xxxx
499:   LRP TEMP[11].x, TEMP[10].yyyy, TEMP[12].xxxx, TEMP[11].xxxx
500:   ADD TEMP[12].x, TEMP[9].xxxx, IMM[5].wwww
501:   SIN TEMP[12].x, TEMP[12].xxxx
502:   MUL TEMP[12].x, TEMP[12].xxxx, IMM[6].zzzz
503:   FRC TEMP[12].x, TEMP[12].xxxx
504:   ADD TEMP[13].x, TEMP[9].xxxx, IMM[7].yyyy
505:   SIN TEMP[13].x, TEMP[13].xxxx
506:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
507:   FRC TEMP[13].x, TEMP[13].xxxx
508:   LRP TEMP[12].x, TEMP[10].xxxx, TEMP[13].xxxx, TEMP[12].xxxx
509:   ADD TEMP[13].x, TEMP[9].xxxx, IMM[7].zzzz
510:   SIN TEMP[13].x, TEMP[13].xxxx
511:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
512:   FRC TEMP[13].x, TEMP[13].xxxx
513:   ADD TEMP[9].x, TEMP[9].xxxx, IMM[7].wwww
514:   SIN TEMP[9].x, TEMP[9].xxxx
515:   MUL TEMP[9].x, TEMP[9].xxxx, IMM[6].zzzz
516:   FRC TEMP[9].x, TEMP[9].xxxx
517:   LRP TEMP[9].x, TEMP[10].xxxx, TEMP[9].xxxx, TEMP[13].xxxx
518:   LRP TEMP[9].x, TEMP[10].yyyy, TEMP[9].xxxx, TEMP[12].xxxx
519:   LRP TEMP[9].x, TEMP[10].zzzz, TEMP[9].xxxx, TEMP[11].xxxx
520:   MAD TEMP[8].x, IMM[11].zzzz, TEMP[9].xxxx, TEMP[8].xxxx
521:   MUL TEMP[9].xyz, IMM[10].xyzz, TEMP[7].xxxx
522:   MAD TEMP[9].xyz, IMM[9].xyzz, TEMP[7].yyyy, TEMP[9].xyzz
523:   MAD TEMP[9].xyz, IMM[8].xyzz, TEMP[7].zzzz, TEMP[9].xyzz
524:   MUL TEMP[7].xyz, TEMP[9].xyzz, IMM[8].wwww
525:   FLR TEMP[9].xyz, TEMP[7].xyzz
526:   FRC TEMP[7].xyz, TEMP[7].xyzz
527:   MUL TEMP[10].xyz, TEMP[7].xyzz, TEMP[7].xyzz
528:   MUL TEMP[7].xyz, IMM[0].xxxx, TEMP[7].xyzz
529:   ADD TEMP[7].xyz, IMM[5].zzzz, -TEMP[7].xyzz
530:   MUL TEMP[7].xyz, TEMP[10].xyzz, TEMP[7].xyzz
531:   MAD TEMP[10].x, TEMP[9].yyyy, IMM[6].xxxx, TEMP[9].xxxx
532:   MAD TEMP[9].x, IMM[5].wwww, TEMP[9].zzzz, TEMP[10].xxxx
533:   SIN TEMP[10].x, TEMP[9].xxxx
534:   MUL TEMP[10].x, TEMP[10].xxxx, IMM[6].zzzz
535:   FRC TEMP[10].x, TEMP[10].xxxx
536:   ADD TEMP[11].x, TEMP[9].xxxx, IMM[6].wwww
537:   SIN TEMP[11].x, TEMP[11].xxxx
538:   MUL TEMP[11].x, TEMP[11].xxxx, IMM[6].zzzz
539:   FRC TEMP[11].x, TEMP[11].xxxx
540:   LRP TEMP[10].x, TEMP[7].xxxx, TEMP[11].xxxx, TEMP[10].xxxx
541:   ADD TEMP[11].x, TEMP[9].xxxx, IMM[6].xxxx
542:   SIN TEMP[11].x, TEMP[11].xxxx
543:   MUL TEMP[11].x, TEMP[11].xxxx, IMM[6].zzzz
544:   FRC TEMP[11].x, TEMP[11].xxxx
545:   ADD TEMP[12].x, TEMP[9].xxxx, IMM[7].xxxx
546:   SIN TEMP[12].x, TEMP[12].xxxx
547:   MUL TEMP[12].x, TEMP[12].xxxx, IMM[6].zzzz
548:   FRC TEMP[12].x, TEMP[12].xxxx
549:   LRP TEMP[11].x, TEMP[7].xxxx, TEMP[12].xxxx, TEMP[11].xxxx
550:   LRP TEMP[10].x, TEMP[7].yyyy, TEMP[11].xxxx, TEMP[10].xxxx
551:   ADD TEMP[11].x, TEMP[9].xxxx, IMM[5].wwww
552:   SIN TEMP[11].x, TEMP[11].xxxx
553:   MUL TEMP[11].x, TEMP[11].xxxx, IMM[6].zzzz
554:   FRC TEMP[11].x, TEMP[11].xxxx
555:   ADD TEMP[12].x, TEMP[9].xxxx, IMM[7].yyyy
556:   SIN TEMP[12].x, TEMP[12].xxxx
557:   MUL TEMP[12].x, TEMP[12].xxxx, IMM[6].zzzz
558:   FRC TEMP[12].x, TEMP[12].xxxx
559:   LRP TEMP[11].x, TEMP[7].xxxx, TEMP[12].xxxx, TEMP[11].xxxx
560:   ADD TEMP[12].x, TEMP[9].xxxx, IMM[7].zzzz
561:   SIN TEMP[12].x, TEMP[12].xxxx
562:   MUL TEMP[12].x, TEMP[12].xxxx, IMM[6].zzzz
563:   FRC TEMP[12].x, TEMP[12].xxxx
564:   ADD TEMP[9].x, TEMP[9].xxxx, IMM[7].wwww
565:   SIN TEMP[9].x, TEMP[9].xxxx
566:   MUL TEMP[9].x, TEMP[9].xxxx, IMM[6].zzzz
567:   FRC TEMP[9].x, TEMP[9].xxxx
568:   LRP TEMP[9].x, TEMP[7].xxxx, TEMP[9].xxxx, TEMP[12].xxxx
569:   LRP TEMP[9].x, TEMP[7].yyyy, TEMP[9].xxxx, TEMP[11].xxxx
570:   LRP TEMP[7].x, TEMP[7].zzzz, TEMP[9].xxxx, TEMP[10].xxxx
571:   ABS TEMP[7].x, TEMP[7].xxxx
572:   MAD TEMP[8].x, IMM[11].wwww, TEMP[7].xxxx, TEMP[8].xxxx
573:   MAD TEMP[6].x, IMM[16].zzzz, TEMP[8].xxxx, TEMP[6].xxxx
574:   ADD TEMP[7].x, TEMP[2].xxxx, IMM[16].wwww
575:   MOV TEMP[7].y, TEMP[2].yyyy
576:   MOV TEMP[7].z, TEMP[2].zzzz
577:   DP3 TEMP[8].x, TEMP[7].xyzz, TEMP[7].xyzz
578:   RSQ TEMP[9].x, TEMP[8].xxxx
579:   MUL TEMP[9].x, TEMP[9].xxxx, TEMP[8].xxxx
580:   CMP TEMP[9].x, -TEMP[8].xxxx, TEMP[9].xxxx, IMM[0].zzzz
581:   ADD TEMP[8].x, TEMP[9].xxxx, IMM[2].zzzz
582:   MUL TEMP[7].xyz, TEMP[7].xyzz, IMM[2].wwww
583:   MAD TEMP[7].xyz, IMM[5].xyyy, CONST[1].xxxx, TEMP[7].xyzz
584:   FLR TEMP[9].xyz, TEMP[7].xyzz
585:   FRC TEMP[10].xyz, TEMP[7].xyzz
586:   MUL TEMP[11].xyz, TEMP[10].xyzz, TEMP[10].xyzz
587:   MUL TEMP[10].xyz, IMM[0].xxxx, TEMP[10].xyzz
588:   ADD TEMP[10].xyz, IMM[5].zzzz, -TEMP[10].xyzz
589:   MUL TEMP[10].xyz, TEMP[11].xyzz, TEMP[10].xyzz
590:   MAD TEMP[11].x, TEMP[9].yyyy, IMM[6].xxxx, TEMP[9].xxxx
591:   MAD TEMP[9].x, IMM[5].wwww, TEMP[9].zzzz, TEMP[11].xxxx
592:   SIN TEMP[11].x, TEMP[9].xxxx
593:   MUL TEMP[11].x, TEMP[11].xxxx, IMM[6].zzzz
594:   FRC TEMP[11].x, TEMP[11].xxxx
595:   ADD TEMP[12].x, TEMP[9].xxxx, IMM[6].wwww
596:   SIN TEMP[12].x, TEMP[12].xxxx
597:   MUL TEMP[12].x, TEMP[12].xxxx, IMM[6].zzzz
598:   FRC TEMP[12].x, TEMP[12].xxxx
599:   LRP TEMP[11].x, TEMP[10].xxxx, TEMP[12].xxxx, TEMP[11].xxxx
600:   ADD TEMP[12].x, TEMP[9].xxxx, IMM[6].xxxx
601:   SIN TEMP[12].x, TEMP[12].xxxx
602:   MUL TEMP[12].x, TEMP[12].xxxx, IMM[6].zzzz
603:   FRC TEMP[12].x, TEMP[12].xxxx
604:   ADD TEMP[13].x, TEMP[9].xxxx, IMM[7].xxxx
605:   SIN TEMP[13].x, TEMP[13].xxxx
606:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
607:   FRC TEMP[13].x, TEMP[13].xxxx
608:   LRP TEMP[12].x, TEMP[10].xxxx, TEMP[13].xxxx, TEMP[12].xxxx
609:   LRP TEMP[11].x, TEMP[10].yyyy, TEMP[12].xxxx, TEMP[11].xxxx
610:   ADD TEMP[12].x, TEMP[9].xxxx, IMM[5].wwww
611:   SIN TEMP[12].x, TEMP[12].xxxx
612:   MUL TEMP[12].x, TEMP[12].xxxx, IMM[6].zzzz
613:   FRC TEMP[12].x, TEMP[12].xxxx
614:   ADD TEMP[13].x, TEMP[9].xxxx, IMM[7].yyyy
615:   SIN TEMP[13].x, TEMP[13].xxxx
616:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
617:   FRC TEMP[13].x, TEMP[13].xxxx
618:   LRP TEMP[12].x, TEMP[10].xxxx, TEMP[13].xxxx, TEMP[12].xxxx
619:   ADD TEMP[13].x, TEMP[9].xxxx, IMM[7].zzzz
620:   SIN TEMP[13].x, TEMP[13].xxxx
621:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
622:   FRC TEMP[13].x, TEMP[13].xxxx
623:   ADD TEMP[9].x, TEMP[9].xxxx, IMM[7].wwww
624:   SIN TEMP[9].x, TEMP[9].xxxx
625:   MUL TEMP[9].x, TEMP[9].xxxx, IMM[6].zzzz
626:   FRC TEMP[9].x, TEMP[9].xxxx
627:   LRP TEMP[9].x, TEMP[10].xxxx, TEMP[9].xxxx, TEMP[13].xxxx
628:   LRP TEMP[9].x, TEMP[10].yyyy, TEMP[9].xxxx, TEMP[12].xxxx
629:   LRP TEMP[9].x, TEMP[10].zzzz, TEMP[9].xxxx, TEMP[11].xxxx
630:   MUL TEMP[9].x, IMM[6].yyyy, TEMP[9].xxxx
631:   MUL TEMP[10].xyz, IMM[10].xyzz, TEMP[7].xxxx
632:   MAD TEMP[10].xyz, IMM[9].xyzz, TEMP[7].yyyy, TEMP[10].xyzz
633:   MAD TEMP[10].xyz, IMM[8].xyzz, TEMP[7].zzzz, TEMP[10].xyzz
634:   MUL TEMP[7].xyz, TEMP[10].xyzz, IMM[8].wwww
635:   FLR TEMP[10].xyz, TEMP[7].xyzz
636:   FRC TEMP[11].xyz, TEMP[7].xyzz
637:   MUL TEMP[12].xyz, TEMP[11].xyzz, TEMP[11].xyzz
638:   MUL TEMP[11].xyz, IMM[0].xxxx, TEMP[11].xyzz
639:   ADD TEMP[11].xyz, IMM[5].zzzz, -TEMP[11].xyzz
640:   MUL TEMP[11].xyz, TEMP[12].xyzz, TEMP[11].xyzz
641:   MAD TEMP[12].x, TEMP[10].yyyy, IMM[6].xxxx, TEMP[10].xxxx
642:   MAD TEMP[10].x, IMM[5].wwww, TEMP[10].zzzz, TEMP[12].xxxx
643:   SIN TEMP[12].x, TEMP[10].xxxx
644:   MUL TEMP[12].x, TEMP[12].xxxx, IMM[6].zzzz
645:   FRC TEMP[12].x, TEMP[12].xxxx
646:   ADD TEMP[13].x, TEMP[10].xxxx, IMM[6].wwww
647:   SIN TEMP[13].x, TEMP[13].xxxx
648:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
649:   FRC TEMP[13].x, TEMP[13].xxxx
650:   LRP TEMP[12].x, TEMP[11].xxxx, TEMP[13].xxxx, TEMP[12].xxxx
651:   ADD TEMP[13].x, TEMP[10].xxxx, IMM[6].xxxx
652:   SIN TEMP[13].x, TEMP[13].xxxx
653:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
654:   FRC TEMP[13].x, TEMP[13].xxxx
655:   ADD TEMP[14].x, TEMP[10].xxxx, IMM[7].xxxx
656:   SIN TEMP[14].x, TEMP[14].xxxx
657:   MUL TEMP[14].x, TEMP[14].xxxx, IMM[6].zzzz
658:   FRC TEMP[14].x, TEMP[14].xxxx
659:   LRP TEMP[13].x, TEMP[11].xxxx, TEMP[14].xxxx, TEMP[13].xxxx
660:   LRP TEMP[12].x, TEMP[11].yyyy, TEMP[13].xxxx, TEMP[12].xxxx
661:   ADD TEMP[13].x, TEMP[10].xxxx, IMM[5].wwww
662:   SIN TEMP[13].x, TEMP[13].xxxx
663:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
664:   FRC TEMP[13].x, TEMP[13].xxxx
665:   ADD TEMP[14].x, TEMP[10].xxxx, IMM[7].yyyy
666:   SIN TEMP[14].x, TEMP[14].xxxx
667:   MUL TEMP[14].x, TEMP[14].xxxx, IMM[6].zzzz
668:   FRC TEMP[14].x, TEMP[14].xxxx
669:   LRP TEMP[13].x, TEMP[11].xxxx, TEMP[14].xxxx, TEMP[13].xxxx
670:   ADD TEMP[14].x, TEMP[10].xxxx, IMM[7].zzzz
671:   SIN TEMP[14].x, TEMP[14].xxxx
672:   MUL TEMP[14].x, TEMP[14].xxxx, IMM[6].zzzz
673:   FRC TEMP[14].x, TEMP[14].xxxx
674:   ADD TEMP[10].x, TEMP[10].xxxx, IMM[7].wwww
675:   SIN TEMP[10].x, TEMP[10].xxxx
676:   MUL TEMP[10].x, TEMP[10].xxxx, IMM[6].zzzz
677:   FRC TEMP[10].x, TEMP[10].xxxx
678:   LRP TEMP[10].x, TEMP[11].xxxx, TEMP[10].xxxx, TEMP[14].xxxx
679:   LRP TEMP[10].x, TEMP[11].yyyy, TEMP[10].xxxx, TEMP[13].xxxx
680:   LRP TEMP[10].x, TEMP[11].zzzz, TEMP[10].xxxx, TEMP[12].xxxx
681:   MAD TEMP[9].x, IMM[9].wwww, TEMP[10].xxxx, TEMP[9].xxxx
682:   MUL TEMP[10].xyz, IMM[10].xyzz, TEMP[7].xxxx
683:   MAD TEMP[10].xyz, IMM[9].xyzz, TEMP[7].yyyy, TEMP[10].xyzz
684:   MAD TEMP[10].xyz, IMM[8].xyzz, TEMP[7].zzzz, TEMP[10].xyzz
685:   MUL TEMP[7].xyz, TEMP[10].xyzz, IMM[10].wwww
686:   FLR TEMP[10].xyz, TEMP[7].xyzz
687:   FRC TEMP[11].xyz, TEMP[7].xyzz
688:   MUL TEMP[12].xyz, TEMP[11].xyzz, TEMP[11].xyzz
689:   MUL TEMP[11].xyz, IMM[0].xxxx, TEMP[11].xyzz
690:   ADD TEMP[11].xyz, IMM[5].zzzz, -TEMP[11].xyzz
691:   MUL TEMP[11].xyz, TEMP[12].xyzz, TEMP[11].xyzz
692:   MAD TEMP[12].x, TEMP[10].yyyy, IMM[6].xxxx, TEMP[10].xxxx
693:   MAD TEMP[10].x, IMM[5].wwww, TEMP[10].zzzz, TEMP[12].xxxx
694:   SIN TEMP[12].x, TEMP[10].xxxx
695:   MUL TEMP[12].x, TEMP[12].xxxx, IMM[6].zzzz
696:   FRC TEMP[12].x, TEMP[12].xxxx
697:   ADD TEMP[13].x, TEMP[10].xxxx, IMM[6].wwww
698:   SIN TEMP[13].x, TEMP[13].xxxx
699:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
700:   FRC TEMP[13].x, TEMP[13].xxxx
701:   LRP TEMP[12].x, TEMP[11].xxxx, TEMP[13].xxxx, TEMP[12].xxxx
702:   ADD TEMP[13].x, TEMP[10].xxxx, IMM[6].xxxx
703:   SIN TEMP[13].x, TEMP[13].xxxx
704:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
705:   FRC TEMP[13].x, TEMP[13].xxxx
706:   ADD TEMP[14].x, TEMP[10].xxxx, IMM[7].xxxx
707:   SIN TEMP[14].x, TEMP[14].xxxx
708:   MUL TEMP[14].x, TEMP[14].xxxx, IMM[6].zzzz
709:   FRC TEMP[14].x, TEMP[14].xxxx
710:   LRP TEMP[13].x, TEMP[11].xxxx, TEMP[14].xxxx, TEMP[13].xxxx
711:   LRP TEMP[12].x, TEMP[11].yyyy, TEMP[13].xxxx, TEMP[12].xxxx
712:   ADD TEMP[13].x, TEMP[10].xxxx, IMM[5].wwww
713:   SIN TEMP[13].x, TEMP[13].xxxx
714:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
715:   FRC TEMP[13].x, TEMP[13].xxxx
716:   ADD TEMP[14].x, TEMP[10].xxxx, IMM[7].yyyy
717:   SIN TEMP[14].x, TEMP[14].xxxx
718:   MUL TEMP[14].x, TEMP[14].xxxx, IMM[6].zzzz
719:   FRC TEMP[14].x, TEMP[14].xxxx
720:   LRP TEMP[13].x, TEMP[11].xxxx, TEMP[14].xxxx, TEMP[13].xxxx
721:   ADD TEMP[14].x, TEMP[10].xxxx, IMM[7].zzzz
722:   SIN TEMP[14].x, TEMP[14].xxxx
723:   MUL TEMP[14].x, TEMP[14].xxxx, IMM[6].zzzz
724:   FRC TEMP[14].x, TEMP[14].xxxx
725:   ADD TEMP[10].x, TEMP[10].xxxx, IMM[7].wwww
726:   SIN TEMP[10].x, TEMP[10].xxxx
727:   MUL TEMP[10].x, TEMP[10].xxxx, IMM[6].zzzz
728:   FRC TEMP[10].x, TEMP[10].xxxx
729:   LRP TEMP[10].x, TEMP[11].xxxx, TEMP[10].xxxx, TEMP[14].xxxx
730:   LRP TEMP[10].x, TEMP[11].yyyy, TEMP[10].xxxx, TEMP[13].xxxx
731:   LRP TEMP[10].x, TEMP[11].zzzz, TEMP[10].xxxx, TEMP[12].xxxx
732:   MAD TEMP[9].x, IMM[11].xxxx, TEMP[10].xxxx, TEMP[9].xxxx
733:   MUL TEMP[10].xyz, IMM[10].xyzz, TEMP[7].xxxx
734:   MAD TEMP[10].xyz, IMM[9].xyzz, TEMP[7].yyyy, TEMP[10].xyzz
735:   MAD TEMP[10].xyz, IMM[8].xyzz, TEMP[7].zzzz, TEMP[10].xyzz
736:   MUL TEMP[7].xyz, TEMP[10].xyzz, IMM[11].yyyy
737:   FLR TEMP[10].xyz, TEMP[7].xyzz
738:   FRC TEMP[11].xyz, TEMP[7].xyzz
739:   MUL TEMP[12].xyz, TEMP[11].xyzz, TEMP[11].xyzz
740:   MUL TEMP[11].xyz, IMM[0].xxxx, TEMP[11].xyzz
741:   ADD TEMP[11].xyz, IMM[5].zzzz, -TEMP[11].xyzz
742:   MUL TEMP[11].xyz, TEMP[12].xyzz, TEMP[11].xyzz
743:   MAD TEMP[12].x, TEMP[10].yyyy, IMM[6].xxxx, TEMP[10].xxxx
744:   MAD TEMP[10].x, IMM[5].wwww, TEMP[10].zzzz, TEMP[12].xxxx
745:   SIN TEMP[12].x, TEMP[10].xxxx
746:   MUL TEMP[12].x, TEMP[12].xxxx, IMM[6].zzzz
747:   FRC TEMP[12].x, TEMP[12].xxxx
748:   ADD TEMP[13].x, TEMP[10].xxxx, IMM[6].wwww
749:   SIN TEMP[13].x, TEMP[13].xxxx
750:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
751:   FRC TEMP[13].x, TEMP[13].xxxx
752:   LRP TEMP[12].x, TEMP[11].xxxx, TEMP[13].xxxx, TEMP[12].xxxx
753:   ADD TEMP[13].x, TEMP[10].xxxx, IMM[6].xxxx
754:   SIN TEMP[13].x, TEMP[13].xxxx
755:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
756:   FRC TEMP[13].x, TEMP[13].xxxx
757:   ADD TEMP[14].x, TEMP[10].xxxx, IMM[7].xxxx
758:   SIN TEMP[14].x, TEMP[14].xxxx
759:   MUL TEMP[14].x, TEMP[14].xxxx, IMM[6].zzzz
760:   FRC TEMP[14].x, TEMP[14].xxxx
761:   LRP TEMP[13].x, TEMP[11].xxxx, TEMP[14].xxxx, TEMP[13].xxxx
762:   LRP TEMP[12].x, TEMP[11].yyyy, TEMP[13].xxxx, TEMP[12].xxxx
763:   ADD TEMP[13].x, TEMP[10].xxxx, IMM[5].wwww
764:   SIN TEMP[13].x, TEMP[13].xxxx
765:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
766:   FRC TEMP[13].x, TEMP[13].xxxx
767:   ADD TEMP[14].x, TEMP[10].xxxx, IMM[7].yyyy
768:   SIN TEMP[14].x, TEMP[14].xxxx
769:   MUL TEMP[14].x, TEMP[14].xxxx, IMM[6].zzzz
770:   FRC TEMP[14].x, TEMP[14].xxxx
771:   LRP TEMP[13].x, TEMP[11].xxxx, TEMP[14].xxxx, TEMP[13].xxxx
772:   ADD TEMP[14].x, TEMP[10].xxxx, IMM[7].zzzz
773:   SIN TEMP[14].x, TEMP[14].xxxx
774:   MUL TEMP[14].x, TEMP[14].xxxx, IMM[6].zzzz
775:   FRC TEMP[14].x, TEMP[14].xxxx
776:   ADD TEMP[10].x, TEMP[10].xxxx, IMM[7].wwww
777:   SIN TEMP[10].x, TEMP[10].xxxx
778:   MUL TEMP[10].x, TEMP[10].xxxx, IMM[6].zzzz
779:   FRC TEMP[10].x, TEMP[10].xxxx
780:   LRP TEMP[10].x, TEMP[11].xxxx, TEMP[10].xxxx, TEMP[14].xxxx
781:   LRP TEMP[10].x, TEMP[11].yyyy, TEMP[10].xxxx, TEMP[13].xxxx
782:   LRP TEMP[10].x, TEMP[11].zzzz, TEMP[10].xxxx, TEMP[12].xxxx
783:   MAD TEMP[9].x, IMM[11].zzzz, TEMP[10].xxxx, TEMP[9].xxxx
784:   MUL TEMP[10].xyz, IMM[10].xyzz, TEMP[7].xxxx
785:   MAD TEMP[10].xyz, IMM[9].xyzz, TEMP[7].yyyy, TEMP[10].xyzz
786:   MAD TEMP[10].xyz, IMM[8].xyzz, TEMP[7].zzzz, TEMP[10].xyzz
787:   MUL TEMP[7].xyz, TEMP[10].xyzz, IMM[8].wwww
788:   FLR TEMP[10].xyz, TEMP[7].xyzz
789:   FRC TEMP[7].xyz, TEMP[7].xyzz
790:   MUL TEMP[11].xyz, TEMP[7].xyzz, TEMP[7].xyzz
791:   MUL TEMP[7].xyz, IMM[0].xxxx, TEMP[7].xyzz
792:   ADD TEMP[7].xyz, IMM[5].zzzz, -TEMP[7].xyzz
793:   MUL TEMP[7].xyz, TEMP[11].xyzz, TEMP[7].xyzz
794:   MAD TEMP[11].x, TEMP[10].yyyy, IMM[6].xxxx, TEMP[10].xxxx
795:   MAD TEMP[10].x, IMM[5].wwww, TEMP[10].zzzz, TEMP[11].xxxx
796:   SIN TEMP[11].x, TEMP[10].xxxx
797:   MUL TEMP[11].x, TEMP[11].xxxx, IMM[6].zzzz
798:   FRC TEMP[11].x, TEMP[11].xxxx
799:   ADD TEMP[12].x, TEMP[10].xxxx, IMM[6].wwww
800:   SIN TEMP[12].x, TEMP[12].xxxx
801:   MUL TEMP[12].x, TEMP[12].xxxx, IMM[6].zzzz
802:   FRC TEMP[12].x, TEMP[12].xxxx
803:   LRP TEMP[11].x, TEMP[7].xxxx, TEMP[12].xxxx, TEMP[11].xxxx
804:   ADD TEMP[12].x, TEMP[10].xxxx, IMM[6].xxxx
805:   SIN TEMP[12].x, TEMP[12].xxxx
806:   MUL TEMP[12].x, TEMP[12].xxxx, IMM[6].zzzz
807:   FRC TEMP[12].x, TEMP[12].xxxx
808:   ADD TEMP[13].x, TEMP[10].xxxx, IMM[7].xxxx
809:   SIN TEMP[13].x, TEMP[13].xxxx
810:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
811:   FRC TEMP[13].x, TEMP[13].xxxx
812:   LRP TEMP[12].x, TEMP[7].xxxx, TEMP[13].xxxx, TEMP[12].xxxx
813:   LRP TEMP[11].x, TEMP[7].yyyy, TEMP[12].xxxx, TEMP[11].xxxx
814:   ADD TEMP[12].x, TEMP[10].xxxx, IMM[5].wwww
815:   SIN TEMP[12].x, TEMP[12].xxxx
816:   MUL TEMP[12].x, TEMP[12].xxxx, IMM[6].zzzz
817:   FRC TEMP[12].x, TEMP[12].xxxx
818:   ADD TEMP[13].x, TEMP[10].xxxx, IMM[7].yyyy
819:   SIN TEMP[13].x, TEMP[13].xxxx
820:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
821:   FRC TEMP[13].x, TEMP[13].xxxx
822:   LRP TEMP[12].x, TEMP[7].xxxx, TEMP[13].xxxx, TEMP[12].xxxx
823:   ADD TEMP[13].x, TEMP[10].xxxx, IMM[7].zzzz
824:   SIN TEMP[13].x, TEMP[13].xxxx
825:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
826:   FRC TEMP[13].x, TEMP[13].xxxx
827:   ADD TEMP[10].x, TEMP[10].xxxx, IMM[7].wwww
828:   SIN TEMP[10].x, TEMP[10].xxxx
829:   MUL TEMP[10].x, TEMP[10].xxxx, IMM[6].zzzz
830:   FRC TEMP[10].x, TEMP[10].xxxx
831:   LRP TEMP[10].x, TEMP[7].xxxx, TEMP[10].xxxx, TEMP[13].xxxx
832:   LRP TEMP[10].x, TEMP[7].yyyy, TEMP[10].xxxx, TEMP[12].xxxx
833:   LRP TEMP[7].x, TEMP[7].zzzz, TEMP[10].xxxx, TEMP[11].xxxx
834:   ABS TEMP[7].x, TEMP[7].xxxx
835:   MAD TEMP[9].x, IMM[11].wwww, TEMP[7].xxxx, TEMP[9].xxxx
836:   MAD TEMP[8].x, IMM[16].zzzz, TEMP[9].xxxx, TEMP[8].xxxx
837:   ADD TEMP[7].x, TEMP[8].xxxx, -TEMP[6].xxxx
838:   MOV TEMP[8].x, TEMP[2].xxxx
839:   ADD TEMP[9].x, TEMP[2].yyyy, IMM[16].wwww
840:   MOV TEMP[8].y, TEMP[9].xxxx
841:   MOV TEMP[8].z, TEMP[2].zzzz
842:   DP3 TEMP[9].x, TEMP[8].xyzz, TEMP[8].xyzz
843:   RSQ TEMP[10].x, TEMP[9].xxxx
844:   MUL TEMP[10].x, TEMP[10].xxxx, TEMP[9].xxxx
845:   CMP TEMP[10].x, -TEMP[9].xxxx, TEMP[10].xxxx, IMM[0].zzzz
846:   ADD TEMP[9].x, TEMP[10].xxxx, IMM[2].zzzz
847:   MUL TEMP[8].xyz, TEMP[8].xyzz, IMM[2].wwww
848:   MAD TEMP[8].xyz, IMM[5].xyyy, CONST[1].xxxx, TEMP[8].xyzz
849:   FLR TEMP[10].xyz, TEMP[8].xyzz
850:   FRC TEMP[11].xyz, TEMP[8].xyzz
851:   MUL TEMP[12].xyz, TEMP[11].xyzz, TEMP[11].xyzz
852:   MUL TEMP[11].xyz, IMM[0].xxxx, TEMP[11].xyzz
853:   ADD TEMP[11].xyz, IMM[5].zzzz, -TEMP[11].xyzz
854:   MUL TEMP[11].xyz, TEMP[12].xyzz, TEMP[11].xyzz
855:   MAD TEMP[12].x, TEMP[10].yyyy, IMM[6].xxxx, TEMP[10].xxxx
856:   MAD TEMP[10].x, IMM[5].wwww, TEMP[10].zzzz, TEMP[12].xxxx
857:   SIN TEMP[12].x, TEMP[10].xxxx
858:   MUL TEMP[12].x, TEMP[12].xxxx, IMM[6].zzzz
859:   FRC TEMP[12].x, TEMP[12].xxxx
860:   ADD TEMP[13].x, TEMP[10].xxxx, IMM[6].wwww
861:   SIN TEMP[13].x, TEMP[13].xxxx
862:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
863:   FRC TEMP[13].x, TEMP[13].xxxx
864:   LRP TEMP[12].x, TEMP[11].xxxx, TEMP[13].xxxx, TEMP[12].xxxx
865:   ADD TEMP[13].x, TEMP[10].xxxx, IMM[6].xxxx
866:   SIN TEMP[13].x, TEMP[13].xxxx
867:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
868:   FRC TEMP[13].x, TEMP[13].xxxx
869:   ADD TEMP[14].x, TEMP[10].xxxx, IMM[7].xxxx
870:   SIN TEMP[14].x, TEMP[14].xxxx
871:   MUL TEMP[14].x, TEMP[14].xxxx, IMM[6].zzzz
872:   FRC TEMP[14].x, TEMP[14].xxxx
873:   LRP TEMP[13].x, TEMP[11].xxxx, TEMP[14].xxxx, TEMP[13].xxxx
874:   LRP TEMP[12].x, TEMP[11].yyyy, TEMP[13].xxxx, TEMP[12].xxxx
875:   ADD TEMP[13].x, TEMP[10].xxxx, IMM[5].wwww
876:   SIN TEMP[13].x, TEMP[13].xxxx
877:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
878:   FRC TEMP[13].x, TEMP[13].xxxx
879:   ADD TEMP[14].x, TEMP[10].xxxx, IMM[7].yyyy
880:   SIN TEMP[14].x, TEMP[14].xxxx
881:   MUL TEMP[14].x, TEMP[14].xxxx, IMM[6].zzzz
882:   FRC TEMP[14].x, TEMP[14].xxxx
883:   LRP TEMP[13].x, TEMP[11].xxxx, TEMP[14].xxxx, TEMP[13].xxxx
884:   ADD TEMP[14].x, TEMP[10].xxxx, IMM[7].zzzz
885:   SIN TEMP[14].x, TEMP[14].xxxx
886:   MUL TEMP[14].x, TEMP[14].xxxx, IMM[6].zzzz
887:   FRC TEMP[14].x, TEMP[14].xxxx
888:   ADD TEMP[10].x, TEMP[10].xxxx, IMM[7].wwww
889:   SIN TEMP[10].x, TEMP[10].xxxx
890:   MUL TEMP[10].x, TEMP[10].xxxx, IMM[6].zzzz
891:   FRC TEMP[10].x, TEMP[10].xxxx
892:   LRP TEMP[10].x, TEMP[11].xxxx, TEMP[10].xxxx, TEMP[14].xxxx
893:   LRP TEMP[10].x, TEMP[11].yyyy, TEMP[10].xxxx, TEMP[13].xxxx
894:   LRP TEMP[10].x, TEMP[11].zzzz, TEMP[10].xxxx, TEMP[12].xxxx
895:   MUL TEMP[10].x, IMM[6].yyyy, TEMP[10].xxxx
896:   MUL TEMP[11].xyz, IMM[10].xyzz, TEMP[8].xxxx
897:   MAD TEMP[11].xyz, IMM[9].xyzz, TEMP[8].yyyy, TEMP[11].xyzz
898:   MAD TEMP[11].xyz, IMM[8].xyzz, TEMP[8].zzzz, TEMP[11].xyzz
899:   MUL TEMP[8].xyz, TEMP[11].xyzz, IMM[8].wwww
900:   FLR TEMP[11].xyz, TEMP[8].xyzz
901:   FRC TEMP[12].xyz, TEMP[8].xyzz
902:   MUL TEMP[13].xyz, TEMP[12].xyzz, TEMP[12].xyzz
903:   MUL TEMP[12].xyz, IMM[0].xxxx, TEMP[12].xyzz
904:   ADD TEMP[12].xyz, IMM[5].zzzz, -TEMP[12].xyzz
905:   MUL TEMP[12].xyz, TEMP[13].xyzz, TEMP[12].xyzz
906:   MAD TEMP[13].x, TEMP[11].yyyy, IMM[6].xxxx, TEMP[11].xxxx
907:   MAD TEMP[11].x, IMM[5].wwww, TEMP[11].zzzz, TEMP[13].xxxx
908:   SIN TEMP[13].x, TEMP[11].xxxx
909:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
910:   FRC TEMP[13].x, TEMP[13].xxxx
911:   ADD TEMP[14].x, TEMP[11].xxxx, IMM[6].wwww
912:   SIN TEMP[14].x, TEMP[14].xxxx
913:   MUL TEMP[14].x, TEMP[14].xxxx, IMM[6].zzzz
914:   FRC TEMP[14].x, TEMP[14].xxxx
915:   LRP TEMP[13].x, TEMP[12].xxxx, TEMP[14].xxxx, TEMP[13].xxxx
916:   ADD TEMP[14].x, TEMP[11].xxxx, IMM[6].xxxx
917:   SIN TEMP[14].x, TEMP[14].xxxx
918:   MUL TEMP[14].x, TEMP[14].xxxx, IMM[6].zzzz
919:   FRC TEMP[14].x, TEMP[14].xxxx
920:   ADD TEMP[15].x, TEMP[11].xxxx, IMM[7].xxxx
921:   SIN TEMP[15].x, TEMP[15].xxxx
922:   MUL TEMP[15].x, TEMP[15].xxxx, IMM[6].zzzz
923:   FRC TEMP[15].x, TEMP[15].xxxx
924:   LRP TEMP[14].x, TEMP[12].xxxx, TEMP[15].xxxx, TEMP[14].xxxx
925:   LRP TEMP[13].x, TEMP[12].yyyy, TEMP[14].xxxx, TEMP[13].xxxx
926:   ADD TEMP[14].x, TEMP[11].xxxx, IMM[5].wwww
927:   SIN TEMP[14].x, TEMP[14].xxxx
928:   MUL TEMP[14].x, TEMP[14].xxxx, IMM[6].zzzz
929:   FRC TEMP[14].x, TEMP[14].xxxx
930:   ADD TEMP[15].x, TEMP[11].xxxx, IMM[7].yyyy
931:   SIN TEMP[15].x, TEMP[15].xxxx
932:   MUL TEMP[15].x, TEMP[15].xxxx, IMM[6].zzzz
933:   FRC TEMP[15].x, TEMP[15].xxxx
934:   LRP TEMP[14].x, TEMP[12].xxxx, TEMP[15].xxxx, TEMP[14].xxxx
935:   ADD TEMP[15].x, TEMP[11].xxxx, IMM[7].zzzz
936:   SIN TEMP[15].x, TEMP[15].xxxx
937:   MUL TEMP[15].x, TEMP[15].xxxx, IMM[6].zzzz
938:   FRC TEMP[15].x, TEMP[15].xxxx
939:   ADD TEMP[11].x, TEMP[11].xxxx, IMM[7].wwww
940:   SIN TEMP[11].x, TEMP[11].xxxx
941:   MUL TEMP[11].x, TEMP[11].xxxx, IMM[6].zzzz
942:   FRC TEMP[11].x, TEMP[11].xxxx
943:   LRP TEMP[11].x, TEMP[12].xxxx, TEMP[11].xxxx, TEMP[15].xxxx
944:   LRP TEMP[11].x, TEMP[12].yyyy, TEMP[11].xxxx, TEMP[14].xxxx
945:   LRP TEMP[11].x, TEMP[12].zzzz, TEMP[11].xxxx, TEMP[13].xxxx
946:   MAD TEMP[10].x, IMM[9].wwww, TEMP[11].xxxx, TEMP[10].xxxx
947:   MUL TEMP[11].xyz, IMM[10].xyzz, TEMP[8].xxxx
948:   MAD TEMP[11].xyz, IMM[9].xyzz, TEMP[8].yyyy, TEMP[11].xyzz
949:   MAD TEMP[11].xyz, IMM[8].xyzz, TEMP[8].zzzz, TEMP[11].xyzz
950:   MUL TEMP[8].xyz, TEMP[11].xyzz, IMM[10].wwww
951:   FLR TEMP[11].xyz, TEMP[8].xyzz
952:   FRC TEMP[12].xyz, TEMP[8].xyzz
953:   MUL TEMP[13].xyz, TEMP[12].xyzz, TEMP[12].xyzz
954:   MUL TEMP[12].xyz, IMM[0].xxxx, TEMP[12].xyzz
955:   ADD TEMP[12].xyz, IMM[5].zzzz, -TEMP[12].xyzz
956:   MUL TEMP[12].xyz, TEMP[13].xyzz, TEMP[12].xyzz
957:   MAD TEMP[13].x, TEMP[11].yyyy, IMM[6].xxxx, TEMP[11].xxxx
958:   MAD TEMP[11].x, IMM[5].wwww, TEMP[11].zzzz, TEMP[13].xxxx
959:   SIN TEMP[13].x, TEMP[11].xxxx
960:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
961:   FRC TEMP[13].x, TEMP[13].xxxx
962:   ADD TEMP[14].x, TEMP[11].xxxx, IMM[6].wwww
963:   SIN TEMP[14].x, TEMP[14].xxxx
964:   MUL TEMP[14].x, TEMP[14].xxxx, IMM[6].zzzz
965:   FRC TEMP[14].x, TEMP[14].xxxx
966:   LRP TEMP[13].x, TEMP[12].xxxx, TEMP[14].xxxx, TEMP[13].xxxx
967:   ADD TEMP[14].x, TEMP[11].xxxx, IMM[6].xxxx
968:   SIN TEMP[14].x, TEMP[14].xxxx
969:   MUL TEMP[14].x, TEMP[14].xxxx, IMM[6].zzzz
970:   FRC TEMP[14].x, TEMP[14].xxxx
971:   ADD TEMP[15].x, TEMP[11].xxxx, IMM[7].xxxx
972:   SIN TEMP[15].x, TEMP[15].xxxx
973:   MUL TEMP[15].x, TEMP[15].xxxx, IMM[6].zzzz
974:   FRC TEMP[15].x, TEMP[15].xxxx
975:   LRP TEMP[14].x, TEMP[12].xxxx, TEMP[15].xxxx, TEMP[14].xxxx
976:   LRP TEMP[13].x, TEMP[12].yyyy, TEMP[14].xxxx, TEMP[13].xxxx
977:   ADD TEMP[14].x, TEMP[11].xxxx, IMM[5].wwww
978:   SIN TEMP[14].x, TEMP[14].xxxx
979:   MUL TEMP[14].x, TEMP[14].xxxx, IMM[6].zzzz
980:   FRC TEMP[14].x, TEMP[14].xxxx
981:   ADD TEMP[15].x, TEMP[11].xxxx, IMM[7].yyyy
982:   SIN TEMP[15].x, TEMP[15].xxxx
983:   MUL TEMP[15].x, TEMP[15].xxxx, IMM[6].zzzz
984:   FRC TEMP[15].x, TEMP[15].xxxx
985:   LRP TEMP[14].x, TEMP[12].xxxx, TEMP[15].xxxx, TEMP[14].xxxx
986:   ADD TEMP[15].x, TEMP[11].xxxx, IMM[7].zzzz
987:   SIN TEMP[15].x, TEMP[15].xxxx
988:   MUL TEMP[15].x, TEMP[15].xxxx, IMM[6].zzzz
989:   FRC TEMP[15].x, TEMP[15].xxxx
990:   ADD TEMP[11].x, TEMP[11].xxxx, IMM[7].wwww
991:   SIN TEMP[11].x, TEMP[11].xxxx
992:   MUL TEMP[11].x, TEMP[11].xxxx, IMM[6].zzzz
993:   FRC TEMP[11].x, TEMP[11].xxxx
994:   LRP TEMP[11].x, TEMP[12].xxxx, TEMP[11].xxxx, TEMP[15].xxxx
995:   LRP TEMP[11].x, TEMP[12].yyyy, TEMP[11].xxxx, TEMP[14].xxxx
996:   LRP TEMP[11].x, TEMP[12].zzzz, TEMP[11].xxxx, TEMP[13].xxxx
997:   MAD TEMP[10].x, IMM[11].xxxx, TEMP[11].xxxx, TEMP[10].xxxx
998:   MUL TEMP[11].xyz, IMM[10].xyzz, TEMP[8].xxxx
999:   MAD TEMP[11].xyz, IMM[9].xyzz, TEMP[8].yyyy, TEMP[11].xyzz
1000:   MAD TEMP[11].xyz, IMM[8].xyzz, TEMP[8].zzzz, TEMP[11].xyzz
1001:   MUL TEMP[8].xyz, TEMP[11].xyzz, IMM[11].yyyy
1002:   FLR TEMP[11].xyz, TEMP[8].xyzz
1003:   FRC TEMP[12].xyz, TEMP[8].xyzz
1004:   MUL TEMP[13].xyz, TEMP[12].xyzz, TEMP[12].xyzz
1005:   MUL TEMP[12].xyz, IMM[0].xxxx, TEMP[12].xyzz
1006:   ADD TEMP[12].xyz, IMM[5].zzzz, -TEMP[12].xyzz
1007:   MUL TEMP[12].xyz, TEMP[13].xyzz, TEMP[12].xyzz
1008:   MAD TEMP[13].x, TEMP[11].yyyy, IMM[6].xxxx, TEMP[11].xxxx
1009:   MAD TEMP[11].x, IMM[5].wwww, TEMP[11].zzzz, TEMP[13].xxxx
1010:   SIN TEMP[13].x, TEMP[11].xxxx
1011:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
1012:   FRC TEMP[13].x, TEMP[13].xxxx
1013:   ADD TEMP[14].x, TEMP[11].xxxx, IMM[6].wwww
1014:   SIN TEMP[14].x, TEMP[14].xxxx
1015:   MUL TEMP[14].x, TEMP[14].xxxx, IMM[6].zzzz
1016:   FRC TEMP[14].x, TEMP[14].xxxx
1017:   LRP TEMP[13].x, TEMP[12].xxxx, TEMP[14].xxxx, TEMP[13].xxxx
1018:   ADD TEMP[14].x, TEMP[11].xxxx, IMM[6].xxxx
1019:   SIN TEMP[14].x, TEMP[14].xxxx
1020:   MUL TEMP[14].x, TEMP[14].xxxx, IMM[6].zzzz
1021:   FRC TEMP[14].x, TEMP[14].xxxx
1022:   ADD TEMP[15].x, TEMP[11].xxxx, IMM[7].xxxx
1023:   SIN TEMP[15].x, TEMP[15].xxxx
1024:   MUL TEMP[15].x, TEMP[15].xxxx, IMM[6].zzzz
1025:   FRC TEMP[15].x, TEMP[15].xxxx
1026:   LRP TEMP[14].x, TEMP[12].xxxx, TEMP[15].xxxx, TEMP[14].xxxx
1027:   LRP TEMP[13].x, TEMP[12].yyyy, TEMP[14].xxxx, TEMP[13].xxxx
1028:   ADD TEMP[14].x, TEMP[11].xxxx, IMM[5].wwww
1029:   SIN TEMP[14].x, TEMP[14].xxxx
1030:   MUL TEMP[14].x, TEMP[14].xxxx, IMM[6].zzzz
1031:   FRC TEMP[14].x, TEMP[14].xxxx
1032:   ADD TEMP[15].x, TEMP[11].xxxx, IMM[7].yyyy
1033:   SIN TEMP[15].x, TEMP[15].xxxx
1034:   MUL TEMP[15].x, TEMP[15].xxxx, IMM[6].zzzz
1035:   FRC TEMP[15].x, TEMP[15].xxxx
1036:   LRP TEMP[14].x, TEMP[12].xxxx, TEMP[15].xxxx, TEMP[14].xxxx
1037:   ADD TEMP[15].x, TEMP[11].xxxx, IMM[7].zzzz
1038:   SIN TEMP[15].x, TEMP[15].xxxx
1039:   MUL TEMP[15].x, TEMP[15].xxxx, IMM[6].zzzz
1040:   FRC TEMP[15].x, TEMP[15].xxxx
1041:   ADD TEMP[11].x, TEMP[11].xxxx, IMM[7].wwww
1042:   SIN TEMP[11].x, TEMP[11].xxxx
1043:   MUL TEMP[11].x, TEMP[11].xxxx, IMM[6].zzzz
1044:   FRC TEMP[11].x, TEMP[11].xxxx
1045:   LRP TEMP[11].x, TEMP[12].xxxx, TEMP[11].xxxx, TEMP[15].xxxx
1046:   LRP TEMP[11].x, TEMP[12].yyyy, TEMP[11].xxxx, TEMP[14].xxxx
1047:   LRP TEMP[11].x, TEMP[12].zzzz, TEMP[11].xxxx, TEMP[13].xxxx
1048:   MAD TEMP[10].x, IMM[11].zzzz, TEMP[11].xxxx, TEMP[10].xxxx
1049:   MUL TEMP[11].xyz, IMM[10].xyzz, TEMP[8].xxxx
1050:   MAD TEMP[11].xyz, IMM[9].xyzz, TEMP[8].yyyy, TEMP[11].xyzz
1051:   MAD TEMP[11].xyz, IMM[8].xyzz, TEMP[8].zzzz, TEMP[11].xyzz
1052:   MUL TEMP[8].xyz, TEMP[11].xyzz, IMM[8].wwww
1053:   FLR TEMP[11].xyz, TEMP[8].xyzz
1054:   FRC TEMP[8].xyz, TEMP[8].xyzz
1055:   MUL TEMP[12].xyz, TEMP[8].xyzz, TEMP[8].xyzz
1056:   MUL TEMP[8].xyz, IMM[0].xxxx, TEMP[8].xyzz
1057:   ADD TEMP[8].xyz, IMM[5].zzzz, -TEMP[8].xyzz
1058:   MUL TEMP[8].xyz, TEMP[12].xyzz, TEMP[8].xyzz
1059:   MAD TEMP[12].x, TEMP[11].yyyy, IMM[6].xxxx, TEMP[11].xxxx
1060:   MAD TEMP[11].x, IMM[5].wwww, TEMP[11].zzzz, TEMP[12].xxxx
1061:   SIN TEMP[12].x, TEMP[11].xxxx
1062:   MUL TEMP[12].x, TEMP[12].xxxx, IMM[6].zzzz
1063:   FRC TEMP[12].x, TEMP[12].xxxx
1064:   ADD TEMP[13].x, TEMP[11].xxxx, IMM[6].wwww
1065:   SIN TEMP[13].x, TEMP[13].xxxx
1066:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
1067:   FRC TEMP[13].x, TEMP[13].xxxx
1068:   LRP TEMP[12].x, TEMP[8].xxxx, TEMP[13].xxxx, TEMP[12].xxxx
1069:   ADD TEMP[13].x, TEMP[11].xxxx, IMM[6].xxxx
1070:   SIN TEMP[13].x, TEMP[13].xxxx
1071:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
1072:   FRC TEMP[13].x, TEMP[13].xxxx
1073:   ADD TEMP[14].x, TEMP[11].xxxx, IMM[7].xxxx
1074:   SIN TEMP[14].x, TEMP[14].xxxx
1075:   MUL TEMP[14].x, TEMP[14].xxxx, IMM[6].zzzz
1076:   FRC TEMP[14].x, TEMP[14].xxxx
1077:   LRP TEMP[13].x, TEMP[8].xxxx, TEMP[14].xxxx, TEMP[13].xxxx
1078:   LRP TEMP[12].x, TEMP[8].yyyy, TEMP[13].xxxx, TEMP[12].xxxx
1079:   ADD TEMP[13].x, TEMP[11].xxxx, IMM[5].wwww
1080:   SIN TEMP[13].x, TEMP[13].xxxx
1081:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
1082:   FRC TEMP[13].x, TEMP[13].xxxx
1083:   ADD TEMP[14].x, TEMP[11].xxxx, IMM[7].yyyy
1084:   SIN TEMP[14].x, TEMP[14].xxxx
1085:   MUL TEMP[14].x, TEMP[14].xxxx, IMM[6].zzzz
1086:   FRC TEMP[14].x, TEMP[14].xxxx
1087:   LRP TEMP[13].x, TEMP[8].xxxx, TEMP[14].xxxx, TEMP[13].xxxx
1088:   ADD TEMP[14].x, TEMP[11].xxxx, IMM[7].zzzz
1089:   SIN TEMP[14].x, TEMP[14].xxxx
1090:   MUL TEMP[14].x, TEMP[14].xxxx, IMM[6].zzzz
1091:   FRC TEMP[14].x, TEMP[14].xxxx
1092:   ADD TEMP[11].x, TEMP[11].xxxx, IMM[7].wwww
1093:   SIN TEMP[11].x, TEMP[11].xxxx
1094:   MUL TEMP[11].x, TEMP[11].xxxx, IMM[6].zzzz
1095:   FRC TEMP[11].x, TEMP[11].xxxx
1096:   LRP TEMP[11].x, TEMP[8].xxxx, TEMP[11].xxxx, TEMP[14].xxxx
1097:   LRP TEMP[11].x, TEMP[8].yyyy, TEMP[11].xxxx, TEMP[13].xxxx
1098:   LRP TEMP[8].x, TEMP[8].zzzz, TEMP[11].xxxx, TEMP[12].xxxx
1099:   ABS TEMP[8].x, TEMP[8].xxxx
1100:   MAD TEMP[10].x, IMM[11].wwww, TEMP[8].xxxx, TEMP[10].xxxx
1101:   MAD TEMP[9].x, IMM[16].zzzz, TEMP[10].xxxx, TEMP[9].xxxx
1102:   ADD TEMP[8].x, TEMP[9].xxxx, -TEMP[6].xxxx
1103:   MOV TEMP[7].y, TEMP[8].xxxx
1104:   MOV TEMP[8].x, TEMP[2].xxxx
1105:   MOV TEMP[8].y, TEMP[2].yyyy
1106:   ADD TEMP[2].x, TEMP[2].zzzz, IMM[16].wwww
1107:   MOV TEMP[8].z, TEMP[2].xxxx
1108:   DP3 TEMP[2].x, TEMP[8].xyzz, TEMP[8].xyzz
1109:   RSQ TEMP[9].x, TEMP[2].xxxx
1110:   MUL TEMP[9].x, TEMP[9].xxxx, TEMP[2].xxxx
1111:   CMP TEMP[9].x, -TEMP[2].xxxx, TEMP[9].xxxx, IMM[0].zzzz
1112:   ADD TEMP[2].x, TEMP[9].xxxx, IMM[2].zzzz
1113:   MUL TEMP[8].xyz, TEMP[8].xyzz, IMM[2].wwww
1114:   MAD TEMP[8].xyz, IMM[5].xyyy, CONST[1].xxxx, TEMP[8].xyzz
1115:   FLR TEMP[9].xyz, TEMP[8].xyzz
1116:   FRC TEMP[10].xyz, TEMP[8].xyzz
1117:   MUL TEMP[11].xyz, TEMP[10].xyzz, TEMP[10].xyzz
1118:   MUL TEMP[10].xyz, IMM[0].xxxx, TEMP[10].xyzz
1119:   ADD TEMP[10].xyz, IMM[5].zzzz, -TEMP[10].xyzz
1120:   MUL TEMP[10].xyz, TEMP[11].xyzz, TEMP[10].xyzz
1121:   MAD TEMP[11].x, TEMP[9].yyyy, IMM[6].xxxx, TEMP[9].xxxx
1122:   MAD TEMP[9].x, IMM[5].wwww, TEMP[9].zzzz, TEMP[11].xxxx
1123:   SIN TEMP[11].x, TEMP[9].xxxx
1124:   MUL TEMP[11].x, TEMP[11].xxxx, IMM[6].zzzz
1125:   FRC TEMP[11].x, TEMP[11].xxxx
1126:   ADD TEMP[12].x, TEMP[9].xxxx, IMM[6].wwww
1127:   SIN TEMP[12].x, TEMP[12].xxxx
1128:   MUL TEMP[12].x, TEMP[12].xxxx, IMM[6].zzzz
1129:   FRC TEMP[12].x, TEMP[12].xxxx
1130:   LRP TEMP[11].x, TEMP[10].xxxx, TEMP[12].xxxx, TEMP[11].xxxx
1131:   ADD TEMP[12].x, TEMP[9].xxxx, IMM[6].xxxx
1132:   SIN TEMP[12].x, TEMP[12].xxxx
1133:   MUL TEMP[12].x, TEMP[12].xxxx, IMM[6].zzzz
1134:   FRC TEMP[12].x, TEMP[12].xxxx
1135:   ADD TEMP[13].x, TEMP[9].xxxx, IMM[7].xxxx
1136:   SIN TEMP[13].x, TEMP[13].xxxx
1137:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
1138:   FRC TEMP[13].x, TEMP[13].xxxx
1139:   LRP TEMP[12].x, TEMP[10].xxxx, TEMP[13].xxxx, TEMP[12].xxxx
1140:   LRP TEMP[11].x, TEMP[10].yyyy, TEMP[12].xxxx, TEMP[11].xxxx
1141:   ADD TEMP[12].x, TEMP[9].xxxx, IMM[5].wwww
1142:   SIN TEMP[12].x, TEMP[12].xxxx
1143:   MUL TEMP[12].x, TEMP[12].xxxx, IMM[6].zzzz
1144:   FRC TEMP[12].x, TEMP[12].xxxx
1145:   ADD TEMP[13].x, TEMP[9].xxxx, IMM[7].yyyy
1146:   SIN TEMP[13].x, TEMP[13].xxxx
1147:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
1148:   FRC TEMP[13].x, TEMP[13].xxxx
1149:   LRP TEMP[12].x, TEMP[10].xxxx, TEMP[13].xxxx, TEMP[12].xxxx
1150:   ADD TEMP[13].x, TEMP[9].xxxx, IMM[7].zzzz
1151:   SIN TEMP[13].x, TEMP[13].xxxx
1152:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
1153:   FRC TEMP[13].x, TEMP[13].xxxx
1154:   ADD TEMP[9].x, TEMP[9].xxxx, IMM[7].wwww
1155:   SIN TEMP[9].x, TEMP[9].xxxx
1156:   MUL TEMP[9].x, TEMP[9].xxxx, IMM[6].zzzz
1157:   FRC TEMP[9].x, TEMP[9].xxxx
1158:   LRP TEMP[9].x, TEMP[10].xxxx, TEMP[9].xxxx, TEMP[13].xxxx
1159:   LRP TEMP[9].x, TEMP[10].yyyy, TEMP[9].xxxx, TEMP[12].xxxx
1160:   LRP TEMP[9].x, TEMP[10].zzzz, TEMP[9].xxxx, TEMP[11].xxxx
1161:   MUL TEMP[9].x, IMM[6].yyyy, TEMP[9].xxxx
1162:   MUL TEMP[10].xyz, IMM[10].xyzz, TEMP[8].xxxx
1163:   MAD TEMP[10].xyz, IMM[9].xyzz, TEMP[8].yyyy, TEMP[10].xyzz
1164:   MAD TEMP[10].xyz, IMM[8].xyzz, TEMP[8].zzzz, TEMP[10].xyzz
1165:   MUL TEMP[8].xyz, TEMP[10].xyzz, IMM[8].wwww
1166:   FLR TEMP[10].xyz, TEMP[8].xyzz
1167:   FRC TEMP[11].xyz, TEMP[8].xyzz
1168:   MUL TEMP[12].xyz, TEMP[11].xyzz, TEMP[11].xyzz
1169:   MUL TEMP[11].xyz, IMM[0].xxxx, TEMP[11].xyzz
1170:   ADD TEMP[11].xyz, IMM[5].zzzz, -TEMP[11].xyzz
1171:   MUL TEMP[11].xyz, TEMP[12].xyzz, TEMP[11].xyzz
1172:   MAD TEMP[12].x, TEMP[10].yyyy, IMM[6].xxxx, TEMP[10].xxxx
1173:   MAD TEMP[10].x, IMM[5].wwww, TEMP[10].zzzz, TEMP[12].xxxx
1174:   SIN TEMP[12].x, TEMP[10].xxxx
1175:   MUL TEMP[12].x, TEMP[12].xxxx, IMM[6].zzzz
1176:   FRC TEMP[12].x, TEMP[12].xxxx
1177:   ADD TEMP[13].x, TEMP[10].xxxx, IMM[6].wwww
1178:   SIN TEMP[13].x, TEMP[13].xxxx
1179:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
1180:   FRC TEMP[13].x, TEMP[13].xxxx
1181:   LRP TEMP[12].x, TEMP[11].xxxx, TEMP[13].xxxx, TEMP[12].xxxx
1182:   ADD TEMP[13].x, TEMP[10].xxxx, IMM[6].xxxx
1183:   SIN TEMP[13].x, TEMP[13].xxxx
1184:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
1185:   FRC TEMP[13].x, TEMP[13].xxxx
1186:   ADD TEMP[14].x, TEMP[10].xxxx, IMM[7].xxxx
1187:   SIN TEMP[14].x, TEMP[14].xxxx
1188:   MUL TEMP[14].x, TEMP[14].xxxx, IMM[6].zzzz
1189:   FRC TEMP[14].x, TEMP[14].xxxx
1190:   LRP TEMP[13].x, TEMP[11].xxxx, TEMP[14].xxxx, TEMP[13].xxxx
1191:   LRP TEMP[12].x, TEMP[11].yyyy, TEMP[13].xxxx, TEMP[12].xxxx
1192:   ADD TEMP[13].x, TEMP[10].xxxx, IMM[5].wwww
1193:   SIN TEMP[13].x, TEMP[13].xxxx
1194:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
1195:   FRC TEMP[13].x, TEMP[13].xxxx
1196:   ADD TEMP[14].x, TEMP[10].xxxx, IMM[7].yyyy
1197:   SIN TEMP[14].x, TEMP[14].xxxx
1198:   MUL TEMP[14].x, TEMP[14].xxxx, IMM[6].zzzz
1199:   FRC TEMP[14].x, TEMP[14].xxxx
1200:   LRP TEMP[13].x, TEMP[11].xxxx, TEMP[14].xxxx, TEMP[13].xxxx
1201:   ADD TEMP[14].x, TEMP[10].xxxx, IMM[7].zzzz
1202:   SIN TEMP[14].x, TEMP[14].xxxx
1203:   MUL TEMP[14].x, TEMP[14].xxxx, IMM[6].zzzz
1204:   FRC TEMP[14].x, TEMP[14].xxxx
1205:   ADD TEMP[10].x, TEMP[10].xxxx, IMM[7].wwww
1206:   SIN TEMP[10].x, TEMP[10].xxxx
1207:   MUL TEMP[10].x, TEMP[10].xxxx, IMM[6].zzzz
1208:   FRC TEMP[10].x, TEMP[10].xxxx
1209:   LRP TEMP[10].x, TEMP[11].xxxx, TEMP[10].xxxx, TEMP[14].xxxx
1210:   LRP TEMP[10].x, TEMP[11].yyyy, TEMP[10].xxxx, TEMP[13].xxxx
1211:   LRP TEMP[10].x, TEMP[11].zzzz, TEMP[10].xxxx, TEMP[12].xxxx
1212:   MAD TEMP[9].x, IMM[9].wwww, TEMP[10].xxxx, TEMP[9].xxxx
1213:   MUL TEMP[10].xyz, IMM[10].xyzz, TEMP[8].xxxx
1214:   MAD TEMP[10].xyz, IMM[9].xyzz, TEMP[8].yyyy, TEMP[10].xyzz
1215:   MAD TEMP[10].xyz, IMM[8].xyzz, TEMP[8].zzzz, TEMP[10].xyzz
1216:   MUL TEMP[8].xyz, TEMP[10].xyzz, IMM[10].wwww
1217:   FLR TEMP[10].xyz, TEMP[8].xyzz
1218:   FRC TEMP[11].xyz, TEMP[8].xyzz
1219:   MUL TEMP[12].xyz, TEMP[11].xyzz, TEMP[11].xyzz
1220:   MUL TEMP[11].xyz, IMM[0].xxxx, TEMP[11].xyzz
1221:   ADD TEMP[11].xyz, IMM[5].zzzz, -TEMP[11].xyzz
1222:   MUL TEMP[11].xyz, TEMP[12].xyzz, TEMP[11].xyzz
1223:   MAD TEMP[12].x, TEMP[10].yyyy, IMM[6].xxxx, TEMP[10].xxxx
1224:   MAD TEMP[10].x, IMM[5].wwww, TEMP[10].zzzz, TEMP[12].xxxx
1225:   SIN TEMP[12].x, TEMP[10].xxxx
1226:   MUL TEMP[12].x, TEMP[12].xxxx, IMM[6].zzzz
1227:   FRC TEMP[12].x, TEMP[12].xxxx
1228:   ADD TEMP[13].x, TEMP[10].xxxx, IMM[6].wwww
1229:   SIN TEMP[13].x, TEMP[13].xxxx
1230:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
1231:   FRC TEMP[13].x, TEMP[13].xxxx
1232:   LRP TEMP[12].x, TEMP[11].xxxx, TEMP[13].xxxx, TEMP[12].xxxx
1233:   ADD TEMP[13].x, TEMP[10].xxxx, IMM[6].xxxx
1234:   SIN TEMP[13].x, TEMP[13].xxxx
1235:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
1236:   FRC TEMP[13].x, TEMP[13].xxxx
1237:   ADD TEMP[14].x, TEMP[10].xxxx, IMM[7].xxxx
1238:   SIN TEMP[14].x, TEMP[14].xxxx
1239:   MUL TEMP[14].x, TEMP[14].xxxx, IMM[6].zzzz
1240:   FRC TEMP[14].x, TEMP[14].xxxx
1241:   LRP TEMP[13].x, TEMP[11].xxxx, TEMP[14].xxxx, TEMP[13].xxxx
1242:   LRP TEMP[12].x, TEMP[11].yyyy, TEMP[13].xxxx, TEMP[12].xxxx
1243:   ADD TEMP[13].x, TEMP[10].xxxx, IMM[5].wwww
1244:   SIN TEMP[13].x, TEMP[13].xxxx
1245:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
1246:   FRC TEMP[13].x, TEMP[13].xxxx
1247:   ADD TEMP[14].x, TEMP[10].xxxx, IMM[7].yyyy
1248:   SIN TEMP[14].x, TEMP[14].xxxx
1249:   MUL TEMP[14].x, TEMP[14].xxxx, IMM[6].zzzz
1250:   FRC TEMP[14].x, TEMP[14].xxxx
1251:   LRP TEMP[13].x, TEMP[11].xxxx, TEMP[14].xxxx, TEMP[13].xxxx
1252:   ADD TEMP[14].x, TEMP[10].xxxx, IMM[7].zzzz
1253:   SIN TEMP[14].x, TEMP[14].xxxx
1254:   MUL TEMP[14].x, TEMP[14].xxxx, IMM[6].zzzz
1255:   FRC TEMP[14].x, TEMP[14].xxxx
1256:   ADD TEMP[10].x, TEMP[10].xxxx, IMM[7].wwww
1257:   SIN TEMP[10].x, TEMP[10].xxxx
1258:   MUL TEMP[10].x, TEMP[10].xxxx, IMM[6].zzzz
1259:   FRC TEMP[10].x, TEMP[10].xxxx
1260:   LRP TEMP[10].x, TEMP[11].xxxx, TEMP[10].xxxx, TEMP[14].xxxx
1261:   LRP TEMP[10].x, TEMP[11].yyyy, TEMP[10].xxxx, TEMP[13].xxxx
1262:   LRP TEMP[10].x, TEMP[11].zzzz, TEMP[10].xxxx, TEMP[12].xxxx
1263:   MAD TEMP[9].x, IMM[11].xxxx, TEMP[10].xxxx, TEMP[9].xxxx
1264:   MUL TEMP[10].xyz, IMM[10].xyzz, TEMP[8].xxxx
1265:   MAD TEMP[10].xyz, IMM[9].xyzz, TEMP[8].yyyy, TEMP[10].xyzz
1266:   MAD TEMP[10].xyz, IMM[8].xyzz, TEMP[8].zzzz, TEMP[10].xyzz
1267:   MUL TEMP[8].xyz, TEMP[10].xyzz, IMM[11].yyyy
1268:   FLR TEMP[10].xyz, TEMP[8].xyzz
1269:   FRC TEMP[11].xyz, TEMP[8].xyzz
1270:   MUL TEMP[12].xyz, TEMP[11].xyzz, TEMP[11].xyzz
1271:   MUL TEMP[11].xyz, IMM[0].xxxx, TEMP[11].xyzz
1272:   ADD TEMP[11].xyz, IMM[5].zzzz, -TEMP[11].xyzz
1273:   MUL TEMP[11].xyz, TEMP[12].xyzz, TEMP[11].xyzz
1274:   MAD TEMP[12].x, TEMP[10].yyyy, IMM[6].xxxx, TEMP[10].xxxx
1275:   MAD TEMP[10].x, IMM[5].wwww, TEMP[10].zzzz, TEMP[12].xxxx
1276:   SIN TEMP[12].x, TEMP[10].xxxx
1277:   MUL TEMP[12].x, TEMP[12].xxxx, IMM[6].zzzz
1278:   FRC TEMP[12].x, TEMP[12].xxxx
1279:   ADD TEMP[13].x, TEMP[10].xxxx, IMM[6].wwww
1280:   SIN TEMP[13].x, TEMP[13].xxxx
1281:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
1282:   FRC TEMP[13].x, TEMP[13].xxxx
1283:   LRP TEMP[12].x, TEMP[11].xxxx, TEMP[13].xxxx, TEMP[12].xxxx
1284:   ADD TEMP[13].x, TEMP[10].xxxx, IMM[6].xxxx
1285:   SIN TEMP[13].x, TEMP[13].xxxx
1286:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
1287:   FRC TEMP[13].x, TEMP[13].xxxx
1288:   ADD TEMP[14].x, TEMP[10].xxxx, IMM[7].xxxx
1289:   SIN TEMP[14].x, TEMP[14].xxxx
1290:   MUL TEMP[14].x, TEMP[14].xxxx, IMM[6].zzzz
1291:   FRC TEMP[14].x, TEMP[14].xxxx
1292:   LRP TEMP[13].x, TEMP[11].xxxx, TEMP[14].xxxx, TEMP[13].xxxx
1293:   LRP TEMP[12].x, TEMP[11].yyyy, TEMP[13].xxxx, TEMP[12].xxxx
1294:   ADD TEMP[13].x, TEMP[10].xxxx, IMM[5].wwww
1295:   SIN TEMP[13].x, TEMP[13].xxxx
1296:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
1297:   FRC TEMP[13].x, TEMP[13].xxxx
1298:   ADD TEMP[14].x, TEMP[10].xxxx, IMM[7].yyyy
1299:   SIN TEMP[14].x, TEMP[14].xxxx
1300:   MUL TEMP[14].x, TEMP[14].xxxx, IMM[6].zzzz
1301:   FRC TEMP[14].x, TEMP[14].xxxx
1302:   LRP TEMP[13].x, TEMP[11].xxxx, TEMP[14].xxxx, TEMP[13].xxxx
1303:   ADD TEMP[14].x, TEMP[10].xxxx, IMM[7].zzzz
1304:   SIN TEMP[14].x, TEMP[14].xxxx
1305:   MUL TEMP[14].x, TEMP[14].xxxx, IMM[6].zzzz
1306:   FRC TEMP[14].x, TEMP[14].xxxx
1307:   ADD TEMP[10].x, TEMP[10].xxxx, IMM[7].wwww
1308:   SIN TEMP[10].x, TEMP[10].xxxx
1309:   MUL TEMP[10].x, TEMP[10].xxxx, IMM[6].zzzz
1310:   FRC TEMP[10].x, TEMP[10].xxxx
1311:   LRP TEMP[10].x, TEMP[11].xxxx, TEMP[10].xxxx, TEMP[14].xxxx
1312:   LRP TEMP[10].x, TEMP[11].yyyy, TEMP[10].xxxx, TEMP[13].xxxx
1313:   LRP TEMP[10].x, TEMP[11].zzzz, TEMP[10].xxxx, TEMP[12].xxxx
1314:   MAD TEMP[9].x, IMM[11].zzzz, TEMP[10].xxxx, TEMP[9].xxxx
1315:   MUL TEMP[10].xyz, IMM[10].xyzz, TEMP[8].xxxx
1316:   MAD TEMP[10].xyz, IMM[9].xyzz, TEMP[8].yyyy, TEMP[10].xyzz
1317:   MAD TEMP[10].xyz, IMM[8].xyzz, TEMP[8].zzzz, TEMP[10].xyzz
1318:   MUL TEMP[8].xyz, TEMP[10].xyzz, IMM[8].wwww
1319:   FLR TEMP[10].xyz, TEMP[8].xyzz
1320:   FRC TEMP[8].xyz, TEMP[8].xyzz
1321:   MUL TEMP[11].xyz, TEMP[8].xyzz, TEMP[8].xyzz
1322:   MUL TEMP[8].xyz, IMM[0].xxxx, TEMP[8].xyzz
1323:   ADD TEMP[8].xyz, IMM[5].zzzz, -TEMP[8].xyzz
1324:   MUL TEMP[8].xyz, TEMP[11].xyzz, TEMP[8].xyzz
1325:   MAD TEMP[11].x, TEMP[10].yyyy, IMM[6].xxxx, TEMP[10].xxxx
1326:   MAD TEMP[10].x, IMM[5].wwww, TEMP[10].zzzz, TEMP[11].xxxx
1327:   SIN TEMP[11].x, TEMP[10].xxxx
1328:   MUL TEMP[11].x, TEMP[11].xxxx, IMM[6].zzzz
1329:   FRC TEMP[11].x, TEMP[11].xxxx
1330:   ADD TEMP[12].x, TEMP[10].xxxx, IMM[6].wwww
1331:   SIN TEMP[12].x, TEMP[12].xxxx
1332:   MUL TEMP[12].x, TEMP[12].xxxx, IMM[6].zzzz
1333:   FRC TEMP[12].x, TEMP[12].xxxx
1334:   LRP TEMP[11].x, TEMP[8].xxxx, TEMP[12].xxxx, TEMP[11].xxxx
1335:   ADD TEMP[12].x, TEMP[10].xxxx, IMM[6].xxxx
1336:   SIN TEMP[12].x, TEMP[12].xxxx
1337:   MUL TEMP[12].x, TEMP[12].xxxx, IMM[6].zzzz
1338:   FRC TEMP[12].x, TEMP[12].xxxx
1339:   ADD TEMP[13].x, TEMP[10].xxxx, IMM[7].xxxx
1340:   SIN TEMP[13].x, TEMP[13].xxxx
1341:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
1342:   FRC TEMP[13].x, TEMP[13].xxxx
1343:   LRP TEMP[12].x, TEMP[8].xxxx, TEMP[13].xxxx, TEMP[12].xxxx
1344:   LRP TEMP[11].x, TEMP[8].yyyy, TEMP[12].xxxx, TEMP[11].xxxx
1345:   ADD TEMP[12].x, TEMP[10].xxxx, IMM[5].wwww
1346:   SIN TEMP[12].x, TEMP[12].xxxx
1347:   MUL TEMP[12].x, TEMP[12].xxxx, IMM[6].zzzz
1348:   FRC TEMP[12].x, TEMP[12].xxxx
1349:   ADD TEMP[13].x, TEMP[10].xxxx, IMM[7].yyyy
1350:   SIN TEMP[13].x, TEMP[13].xxxx
1351:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
1352:   FRC TEMP[13].x, TEMP[13].xxxx
1353:   LRP TEMP[12].x, TEMP[8].xxxx, TEMP[13].xxxx, TEMP[12].xxxx
1354:   ADD TEMP[13].x, TEMP[10].xxxx, IMM[7].zzzz
1355:   SIN TEMP[13].x, TEMP[13].xxxx
1356:   MUL TEMP[13].x, TEMP[13].xxxx, IMM[6].zzzz
1357:   FRC TEMP[13].x, TEMP[13].xxxx
1358:   ADD TEMP[10].x, TEMP[10].xxxx, IMM[7].wwww
1359:   SIN TEMP[10].x, TEMP[10].xxxx
1360:   MUL TEMP[10].x, TEMP[10].xxxx, IMM[6].zzzz
1361:   FRC TEMP[10].x, TEMP[10].xxxx
1362:   LRP TEMP[10].x, TEMP[8].xxxx, TEMP[10].xxxx, TEMP[13].xxxx
1363:   LRP TEMP[10].x, TEMP[8].yyyy, TEMP[10].xxxx, TEMP[12].xxxx
1364:   LRP TEMP[8].x, TEMP[8].zzzz, TEMP[10].xxxx, TEMP[11].xxxx
1365:   ABS TEMP[8].x, TEMP[8].xxxx
1366:   MAD TEMP[9].x, IMM[11].wwww, TEMP[8].xxxx, TEMP[9].xxxx
1367:   MAD TEMP[2].x, IMM[16].zzzz, TEMP[9].xxxx, TEMP[2].xxxx
1368:   ADD TEMP[2].x, TEMP[2].xxxx, -TEMP[6].xxxx
1369:   MOV TEMP[7].z, TEMP[2].xxxx
1370:   DP3 TEMP[2].x, TEMP[7].xyzz, TEMP[7].xyzz
1371:   RSQ TEMP[2].x, TEMP[2].xxxx
1372:   MUL TEMP[2].z, TEMP[7].xyzz, TEMP[2].xxxx
1373:   MAD TEMP[2].x, TEMP[2].zzzz, IMM[17].xxxx, IMM[17].yyyy
1374:   MUL TEMP[2].xyz, TEMP[5].xyzz, TEMP[2].xxxx
1375:   ADD TEMP[3].x, TEMP[3].xxxx, IMM[13].wwww
1376:   MUL TEMP[3].x, TEMP[3].xxxx, IMM[0].xxxx
1377:   MOV_SAT TEMP[3].x, TEMP[3].xxxx
1378:   LRP TEMP[4].xyz, TEMP[3].xxxx, TEMP[2].xyzz, TEMP[5].xyzz
1379:   MOV TEMP[1], TEMP[4]
1380: ENDIF
1381: MOV OUT[0], TEMP[1]
1382: END
   );
#endif
