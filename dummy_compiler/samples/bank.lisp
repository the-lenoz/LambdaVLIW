(defun null (pair) (= pair 0))

(defun find_balance (db id)
  (cond ((null db) 0)
        ((= (car (car db)) id) (cdr (car db)))
        (1 (find_balance (cdr db) id))))

(defun update_balance (db id val)
  (cond ((null db) 0)
        ((= (car (car db)) id) 
         (cons (cons id val) (cdr db)))
        (1 (cons (car db) (update_balance (cdr db) id val)))))

(defun deposit (db id amount)
  (update_balance db id (+ (find_balance db id) amount)))

(defun withdraw (db id amount)
  (cond ((> (find_balance db id) amount) (update_balance db id (- (find_balance db id) amount)))
	((= (find_balance db id) amount) (update_balance db id (- (find_balance db id) amount)))
        (1 db)))

(defun transfer (db from to amount)
  (cond ((> (find_balance db from) amount) (deposit (withdraw db from amount) to amount))
	((= (find_balance db from) amount) (deposit (withdraw db from amount) to amount))
        (1 db)))

(defun add_account (db id balance)
  (cons (cons id balance) db))

(defun remove_account (db id)
  (cond ((null db) 0)
        ((= (car (car db)) id) (cdr db))
        (1 (cons (car db) (remove_account (cdr db) id)))))

(defun run_logic (db stage)
  (cond ((= stage 0) db)
        ((= stage 1) (run_logic (remove_account db 2) 0))
        ((= stage 2) (run_logic (transfer db 1 2 500) 1))
        ((= stage 3) (run_logic (deposit db 2 1000) 2))
        ((= stage 4) (run_logic (add_account db 2 0) 3))
        ((= stage 5) (run_logic (add_account 0 1 2000) 4))
        (1 (run_logic db 5))))

(defun print_pair (pair) (let ((_ (print_int (car pair))) (_ (print_int (cdr pair)))) 0))

(defun main (argc argv) (print_pair (car (run_logic 0 5))))
