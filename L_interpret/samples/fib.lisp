(defun fib (n)
  (cond
    ((< n 3) 1)
    (1 (+
	 (fib (- n 1))
	 (fib (- n 2))))))

(defun printfib (n) (print (fib n)))

(defun foreach (list) (cond ((quote list) (let ((_ (print (car list)))) (foreach (cdr list))))
			      (1 nil)))

(print (car (12 2 2)))
(foreach (1 2))
