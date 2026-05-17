(defun range (start stop)
  (cond ((= start stop) 0)
	((> start stop) 0)
	(1 (cons start (range (+ start 1) stop)))))

(defun print_list (list)
  (cond ((= list 0) 0)
	(1 (let ((_ (print_int (car list))) (_ (print_list (cdr list)))) 0))))

(defun main (argc argv)
  (print_list (range 0 100)))
