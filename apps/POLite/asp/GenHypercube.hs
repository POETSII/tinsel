import System.Environment

type Vertex = [Int]
type Edge = (Vertex, Vertex)
type Graph = ([Vertex], [Edge])

extend :: Int -> Graph -> Graph
extend n (vs, es) = (concat vs', es')
  where
    vs' = [map (i:) vs | i <- [0..n-1]]
    es' = concat [[(i:src, i:dst) | (src, dst) <- es] | i <- [0..n-1]]
       ++ connect vs'

    connect [] = []
    connect [vs] = []
    connect (vs:ws:rest) =
      zip vs ws ++ connect (ws:rest)

empty :: Graph
empty = ([[]], [])

encode :: Int -> [Int] -> Int
encode n xs = sum [(n^d) * x | (d, x) <- zip [0..] xs]

hypercube :: Int -> Int -> Graph
hypercube dims n = gs !! dims
  where gs = iterate (extend n) empty

render :: Int -> Graph -> IO ()
render n (vs, es) =
  sequence_ 
    [ do putStr (show (encode n src))
         putStr " "
         putStrLn (show (encode n dst))
         putStr (show (encode n dst))
         putStr " "
         putStrLn (show (encode n src))
    | (src, dst) <- es
    ]

main :: IO ()
main = do
  [d, n] <- getArgs
  let dims = read d :: Int
  let size = read n :: Int
  let g = hypercube dims size
  render size (hypercube dims size)
