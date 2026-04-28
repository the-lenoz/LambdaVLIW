(defun fib-recursive (n current next)
  (cond
    ((= n 0) current)
    (1 (fib-recursive (- n 1) next (+ current next)))))

(defun calculate-fib (n)
  (cond
    ((= n 0) 0)
    (1 (fib-recursive n 0 1))))

(defun sum-fib-range (n accumulator)
  (cond
    ((= n 0) accumulator)
    (1 (sum-fib-range (- n 1) (+ accumulator (calculate-fib n))))))

(defun main ()
  (sum-fib-range 9 0))
