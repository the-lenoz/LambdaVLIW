(defun fact (n)
  (cond
    ((< n 2) 1)
    (1 (* n (fact (- n 1))))))

(print (fact 5))
(print (fact 10))
