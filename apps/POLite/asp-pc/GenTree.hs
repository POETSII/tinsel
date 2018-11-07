import System.Environment

type Vertex = Int
type Edge = (Vertex, Vertex)
type Graph = ([Vertex], [Edge])

-- Size of balanced tree of depth d and arity n
size :: Int -> Int -> Int
size 0 n = 1
size d n = 1 + n * size (d-1) n

-- Balanced tree of depth d and arity n
tree :: Int -> Int -> Graph
tree d n = (vs, es)
  where
    vs = [0 .. size d n - 1]
    ws = [0 .. size (d-1) n - 1]
    es = concat [ [(w, n*w+o) | o <- [1..n]]
                | w <- ws ]

render :: Graph -> IO ()
render (vs, es) =
  sequence_ 
    [ do putStr (show src)
         putStr " "
         putStrLn (show dst)
         putStr (show dst)
         putStr " "
         putStrLn (show src)
    | (src, dst) <- es
    ]

main :: IO ()
main = do
  [d, n] <- getArgs
  let depth = read d :: Int
  let arity = read n :: Int
  let g = tree depth arity
  render g
