; ModuleID = 'fence_test_module'
source_filename = "fence_test.c"

define void @complicated_flow(ptr %data, ptr %ready, i32 %val) {
entry:
  %cmp = icmp sgt i32 %val, 0
  br i1 %cmp, label %loop_header, label %exit

loop_header:                                      ; preds = %loop_latch, %entry
  %counter = phi i32 [ 0, %entry ], [ %next_count, %loop_latch ]
  
  ; --- Potential TSO/PSO Violation 1: Write-Write Reordering ---
  ; Writing to 'data' then 'ready'. TSO forbids W-W.
  ; PSO allows W-W, so it shouldn't insert a fence here for PSO.
  store i32 %val, ptr %data, align 4
  store i32 1, ptr %ready, align 4
  
  %cond = icmp eq i32 %counter, 10
  br i1 %cond, label %exit, label %loop_latch

loop_latch:                                       ; preds = %loop_header
  ; --- Potential Violation 2: Read-Read Reordering ---
  ; TSO and PSO both forbid R-R reordering[cite: 6, 7].
  %r1 = load i32, ptr %ready, align 4
  %r2 = load i32, ptr %data, align 4
  
  %next_count = add i32 %counter, 1
  br label %loop_header

exit:                                             ; preds = %loop_header, %entry
  ret void
}