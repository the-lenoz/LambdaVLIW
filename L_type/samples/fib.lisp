(defun fib_recursive (n current next)
  (cond
    ((= n 0) current)
    (1 (fib_recursive (- n 1) next (+ current next)))))

(defun calculate_fib (n)
  (cond
    ((= n 0) 0)
    (1 (fib_recursive n 0 1))))

(defun calculate_fib_naive (n)
  (cond
    ((< n 3) 1)
    (1 (+ (calculate_fib_naive (- n 1)) (calculate_fib_naive (- n 2))))))

(defun sum_fib_range (n accumulator)
  (cond
    ((= n 0) accumulator)
    (1 (sum_fib_range (- n 1) (+ accumulator (calculate_fib n))))))

(defun main ()
  (calculate_fib_naive 32))
