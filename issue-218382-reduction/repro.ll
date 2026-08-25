define <4 x i32> @f(i32 %x) {
  %a = and i32 %x, 1
  %c = call i32 @llvm.ctpop.i32(i32 %a)
  %v = insertelement <4 x i32> splat (i32 1), i32 %c, i64 0
  %b = bitcast <4 x i32> %v to <2 x i64>
  %e = extractelement <2 x i64> %b, i64 0
  %idx = trunc i64 %e to i8
  %r = insertelement <4 x i32> zeroinitializer, i32 0, i8 %idx
  ret <4 x i32> %r
}
