package main

import (
	"fmt"
	"math"
	"os"
	"strconv"
	"time"
)

var ZERO int64

func init() { ZERO = int64(len(os.Args)) - 1 }

var PARAM = []int64{30, 2000000, 2000000, 50000, 80000, 300, 200, 80, 50000, 40,
	20000, 500, 110, 10, 200000, 5000000, 400000, 16, 2000000, 100000, 20, 700000}

func fib(n int64) int64 {
	if n < 2 {
		return n
	}
	return fib(n-1) + fib(n-2)
}

func forsum(n int64) int64 {
	var s int64 = 0
	for i := int64(0); i < n; i++ {
		s = s + i*i
	}
	return s
}

func intloop(n int64) int64 {
	var s int64 = 0
	var i int64 = 0
	for i < n {
		s = s + i*i - (i >> 3)
		i = i + 1
	}
	return s
}

func collatz(limit int64) int64 {
	var best, besti int64 = 0, 0
	var i int64 = 1
	for i < limit {
		n := i
		var steps int64 = 0
		for n != 1 {
			if n%2 == 0 {
				n = n / 2
			} else {
				n = 3*n + 1
			}
			steps = steps + 1
		}
		if steps > best {
			best = steps
			besti = i
		}
		i = i + 1
	}
	return besti
}

func primes(n int64) int64 {
	var count int64 = 0
	var i int64 = 2
	for i < n {
		var j int64 = 2
		var isp int64 = 1
		for j*j <= i {
			if i%j == 0 {
				isp = 0
				break
			}
			j = j + 1
		}
		count = count + isp
		i = i + 1
	}
	return count
}

func mandel(w, h, maxit int64) int64 {
	var total int64 = 0
	for y := int64(0); y < h; y++ {
		ci := float64(y)*2.0/float64(h) - 1.0
		for x := int64(0); x < w; x++ {
			cr := float64(x)*3.0/float64(w) - 2.0
			zr, zi := 0.0, 0.0
			var k int64 = 0
			for k < maxit {
				zr2 := zr * zr
				zi2 := zi * zi
				if zr2+zi2 > 4.0 {
					break
				}
				zi = 2.0*zr*zi + ci
				zr = zr2 - zi2 + cr
				k = k + 1
			}
			total = total + k
		}
	}
	return total
}

func listsum(data []int64, reps int64) int64 {
	var s int64 = 0
	n := int64(len(data))
	var r int64 = 0
	for r < reps {
		var i int64 = 0
		for i < n {
			s = s + data[i]
			i = i + 1
		}
		r = r + 1
	}
	return s
}

func nbody(steps int64) float64 {
	const n = 5
	var x, y, vx, vy, m [n]float64
	for i := 0; i < n; i++ {
		x[i] = float64(i) + 1.0
		y[i] = float64(i) * 0.5
		m[i] = 1.0 + float64(i)*0.1
	}
	dt := 0.001
	for s := int64(0); s < steps; s++ {
		for i := 0; i < n; i++ {
			fx, fy := 0.0, 0.0
			for j := 0; j < n; j++ {
				if j != i {
					dx := x[j] - x[i]
					dy := y[j] - y[i]
					d2 := dx*dx + dy*dy + 0.01
					inv := m[j] / (d2 * math.Sqrt(d2))
					fx = fx + dx*inv
					fy = fy + dy*inv
				}
			}
			vx[i] = vx[i] + fx*dt
			vy[i] = vy[i] + fy*dt
		}
		for i := 0; i < n; i++ {
			x[i] = x[i] + vx[i]*dt
			y[i] = y[i] + vy[i]*dt
		}
	}
	e := 0.0
	for i := 0; i < n; i++ {
		e = e + m[i]*(vx[i]*vx[i]+vy[i]*vy[i])
	}
	return e
}

func spectral(n int64) float64 {
	u := make([]float64, n)
	v := make([]float64, n)
	for i := range u {
		u[i] = 1.0
	}
	for it := 0; it < 10; it++ {
		for i := int64(0); i < n; i++ {
			t := 0.0
			for j := int64(0); j < n; j++ {
				t = t + u[j]/float64((i+j)*(i+j+1)/2+i+1)
			}
			v[i] = t
		}
		for i := int64(0); i < n; i++ {
			t := 0.0
			for j := int64(0); j < n; j++ {
				t = t + v[j]/float64((j+i)*(j+i+1)/2+j+1)
			}
			u[i] = t
		}
	}
	a, b := 0.0, 0.0
	for i := int64(0); i < n; i++ {
		a = a + u[i]*v[i]
		b = b + v[i]*v[i]
	}
	return math.Sqrt(a / b)
}

func matmul(n int64) float64 {
	a := make([][]float64, 0, n)
	b := make([][]float64, 0, n)
	for i := int64(0); i < n; i++ {
		ra := make([]float64, n)
		rb := make([]float64, n)
		for j := int64(0); j < n; j++ {
			ra[j] = float64(i*n+j) * 0.5
			rb[j] = float64(j*n+i) * 0.25
		}
		a = append(a, ra)
		b = append(b, rb)
	}
	c := 0.0
	for i := int64(0); i < n; i++ {
		ai := a[i]
		for j := int64(0); j < n; j++ {
			t := 0.0
			for k := int64(0); k < n; k++ {
				t = t + ai[k]*b[k][j]
			}
			c = c + t
		}
	}
	return c
}

func qsolve(cols []int64, row, n int64) int64 {
	if row == n {
		return 1
	}
	var count int64 = 0
	for c := int64(0); c < n; c++ {
		var ok int64 = 1
		for r := int64(0); r < row; r++ {
			d := cols[r] - c
			if d < 0 {
				d = -d
			}
			if cols[r] == c || d == row-r {
				ok = 0
				break
			}
		}
		if ok == 1 {
			cols[row] = c
			count = count + qsolve(cols, row+1, n)
		}
	}
	return count
}

func queens(n int64) int64 {
	cols := make([]int64, n)
	return qsolve(cols, 0, n)
}

func sift(a []int64, root, end int64) {
	for {
		c := root*2 + 1
		if c >= end {
			break
		}
		if c+1 < end && a[c] < a[c+1] {
			c = c + 1
		}
		if a[root] >= a[c] {
			break
		}
		t := a[root]
		a[root] = a[c]
		a[c] = t
		root = c
	}
}

func heapsort(n int64) int64 {
	a := make([]int64, n)
	var seed int64 = 12345
	for i := int64(0); i < n; i++ {
		seed = (seed*1103515245 + 12345) & 0x7FFFFFFF
		a[i] = seed
	}
	for i := n/2 - 1; i >= 0; i-- {
		sift(a, i, n)
	}
	for e := n - 1; e > 0; e-- {
		t := a[0]
		a[0] = a[e]
		a[e] = t
		sift(a, 0, e)
	}
	var s int64 = 0
	for i := int64(0); i < n; i++ {
		s = (s + a[i]*(i+1)) & 0x7FFFFFFF
	}
	return s
}

func bitops(n int64) int64 {
	var x int64 = 1
	for i := int64(0); i < n; i++ {
		x = x ^ (i * 2654435761)
		x = ((x << 7) | (x >> 25)) & 0xFFFFFFFF
		x = x + (x >> 11)
		x = x & 0xFFFFFFFF
	}
	return x
}

func gcdloop(n int64) int64 {
	var s int64 = 0
	for i := int64(1); i < n; i++ {
		a, b := i, n-i
		for b != 0 {
			t := a % b
			a = b
			b = t
		}
		s = s + a
	}
	return s
}

type Node struct {
	l, r *Node
}

func btree(d int64) *Node {
	if d == 0 {
		return &Node{nil, nil}
	}
	return &Node{btree(d - 1), btree(d - 1)}
}

func bcheck(t *Node) int64 {
	if t.l == nil {
		return 1
	}
	return 1 + bcheck(t.l) + bcheck(t.r)
}

func binarytrees(d int64) int64 {
	var s int64 = 0
	for i := 0; i < 4; i++ {
		s = s + bcheck(btree(d))
	}
	return s
}

func sieve(n int64) int64 {
	f := make([]int8, n)
	for i := range f {
		f[i] = 1
	}
	for i := int64(2); i*i < n; i++ {
		if f[i] == 1 {
			for j := i * i; j < n; j += i {
				f[j] = 0
			}
		}
	}
	var c int64 = 0
	for i := int64(2); i < n; i++ {
		c = c + int64(f[i])
	}
	return c
}

func dotprod(n, reps int64) float64 {
	a := make([]float64, n)
	b := make([]float64, n)
	for i := int64(0); i < n; i++ {
		a[i] = float64(i) * 0.5
		b[i] = float64(n-i) * 0.25
	}
	s := 0.0
	for r := int64(0); r < reps; r++ {
		t := 0.0
		for i := int64(0); i < n; i++ {
			t = t + a[i]*b[i]
		}
		s = s + t
	}
	return s
}

func trig(n int64) float64 {
	s := 0.0
	for i := int64(0); i < n; i++ {
		x := float64(i) * 0.0001
		s = s + math.Sin(x)*math.Cos(x) + math.Sqrt(x+1.0)
	}
	return s
}

func fannkuch(n int64) int64 {
	perm := make([]int64, n)
	perm1 := make([]int64, n)
	count := make([]int64, n)
	var i, maxflips, checksum int64
	r := n
	var sign int64 = 1
	for i = 0; i < n; i++ {
		perm1[i] = i
	}
	for {
		for r != 1 {
			count[r-1] = r
			r = r - 1
		}
		for i = 0; i < n; i++ {
			perm[i] = perm1[i]
		}
		var flips int64 = 0
		k := perm[0]
		for k != 0 {
			lo := int64(0)
			hi := k
			for lo < hi {
				t := perm[lo]
				perm[lo] = perm[hi]
				perm[hi] = t
				lo = lo + 1
				hi = hi - 1
			}
			flips = flips + 1
			k = perm[0]
		}
		if flips > maxflips {
			maxflips = flips
		}
		checksum = checksum + sign*flips
		sign = -sign
		for {
			if r == n {
				return checksum*1000000 + maxflips
			}
			p0 := perm1[0]
			for i = 0; i < r; i++ {
				perm1[i] = perm1[i+1]
			}
			perm1[r] = p0
			count[r] = count[r] - 1
			if count[r] > 0 {
				break
			}
			r = r + 1
		}
	}
}

func taylor(n int64) float64 {
	s := 0.0
	sign := 1.0
	var k int64 = 1
	for k <= n {
		s = s + sign/float64(2*k-1)
		sign = -sign
		k = k + 1
	}
	return 4.0 * s
}

func strhash(n int64) int64 {
	var h int64 = 0
	for i := int64(0); i < n; i++ {
		s := strconv.FormatInt(i, 10) + ":" + strconv.FormatInt(i*i, 10)
		for j := 0; j < len(s); j++ {
			h = (h*31 + int64(s[j])) & 0xFFFFFFFF
		}
	}
	return h
}

func dictops(n int64) int64 {
	d := make(map[int64]int64)
	for i := int64(0); i < n; i++ {
		d[i] = i * 7 % n
	}
	var s int64 = 0
	var k int64 = 1
	for i := int64(0); i < n; i++ {
		k = (k*1103515245 + 12345) % n
		s = s + d[k]
	}
	return s
}

var total float64 = 0.0

var sinkI int64
var sinkF float64

func timeI(name string, f func() int64) {
	best := -1.0
	var r int64 = 0
	for t := 0; t < 3; t++ {
		t0 := time.Now()
		r = f()
		dt := float64(time.Since(t0).Nanoseconds()) / 1e6
		sinkI += r + int64(t)
		if best < 0.0 || dt < best {
			best = dt
		}
	}
	total += best
	fmt.Printf("%-14s %10.4f  %d\n", name, best, r)
}

func timeF(name string, f func() float64) {
	best := -1.0
	r := 0.0
	for t := 0; t < 3; t++ {
		t0 := time.Now()
		r = f()
		dt := float64(time.Since(t0).Nanoseconds()) / 1e6
		sinkF += r + float64(t)
		if best < 0.0 || dt < best {
			best = dt
		}
	}
	total += best
	fmt.Printf("%-14s %10.4f  %.6f\n", name, best, r)
}

func main() {
	data := make([]int64, (PARAM[8] + ZERO))
	for i := int64(0); i < (PARAM[8] + ZERO); i++ {
		data[i] = i
	}

	timeI("fib", func() int64 { return fib((PARAM[0] + ZERO)) })
	timeI("forsum", func() int64 { return forsum((PARAM[1] + ZERO)) })
	timeI("intloop", func() int64 { return intloop((PARAM[2] + ZERO)) })
	timeI("collatz", func() int64 { return collatz((PARAM[3] + ZERO)) })
	timeI("primes", func() int64 { return primes((PARAM[4] + ZERO)) })
	timeI("mandel", func() int64 { return mandel((PARAM[5] + ZERO), (PARAM[6] + ZERO), (PARAM[7] + ZERO)) })
	timeI("listsum", func() int64 { return listsum(data, (PARAM[9] + ZERO)) })
	timeF("nbody", func() float64 { return nbody((PARAM[10] + ZERO)) })
	timeF("spectral", func() float64 { return spectral((PARAM[11] + ZERO)) })
	timeF("matmul", func() float64 { return matmul((PARAM[12] + ZERO)) })
	timeI("queens", func() int64 { return queens((PARAM[13] + ZERO)) })
	timeI("heapsort", func() int64 { return heapsort((PARAM[14] + ZERO)) })
	timeI("bitops", func() int64 { return bitops((PARAM[15] + ZERO)) })
	timeI("gcdloop", func() int64 { return gcdloop((PARAM[16] + ZERO)) })
	timeI("binarytrees", func() int64 { return binarytrees((PARAM[17] + ZERO)) })
	timeI("sieve", func() int64 { return sieve((PARAM[18] + ZERO)) })
	timeF("dotprod", func() float64 { return dotprod((PARAM[19] + ZERO), 20) })
	timeF("trig", func() float64 { return trig((PARAM[20] + ZERO) * 50000) })
	timeI("fannkuch", func() int64 { return fannkuch(9 + ZERO) })
	timeF("taylor", func() float64 { return taylor(10000000 + ZERO) })
	timeI("strhash", func() int64 { return strhash((PARAM[21] + ZERO)) })
	timeI("dictops", func() int64 { return dictops((PARAM[21] + ZERO)) })

	fmt.Printf("%-14s %10.2f\n", "TOTAL", total)
	if sinkI == 1 && sinkF == 1.0 {
		fmt.Println("sink")
	}
}
