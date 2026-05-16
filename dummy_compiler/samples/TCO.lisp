(defun heavy_sum (n acc)
  (cond ((< n 1) acc)	
      (1 (heavy_sum (- n 1) (+ n acc)))))

(defun main (argc argv)
  (print_int (heavy_sum 1000000000 0)))
