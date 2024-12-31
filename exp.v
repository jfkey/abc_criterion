module top ( 
	a, b, c, d, f); 
    input a, b, c, d;    // 输入信号
    output f;             // 输出信号
    wire x, y, s, t, u, v, w;           // 中间信号

    assign x = a ^ b;    // 异或操作
    assign y = c & d;    // 与操作
    assign s = x & ~c; 
    assign t = x & ~d;
    assign v = x & y;

    assign w = s & t; 
    assign f = w & v;    // 最终结果
endmodule
