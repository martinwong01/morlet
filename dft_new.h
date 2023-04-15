#define maxN 131072 
#define maxCooleyTukey 25                                    // if prime factor larger than this, use Rader algorithm
#define fft_bit_reverse 1                                    // in-place (1) or out-of-place (0) fft. 
#define align 32

#if fft_bit_reverse == 1
    #include "table.h"
#endif

#if AVX > 0 
    #include <immintrin.h>
#endif

int primeFactors(int,int *);

template <class Type>
void Rader(complex<Type> *,complex<Type> *,complex<Type> *,int,Type);

template <class Type>
void dft_func(complex<Type> *data,complex<Type> *out,int N,int Product,Type pi,int sign) {             // if called from main, set sign=1
    int thread_local i,j,k,m,n,p,q,t;
    alignas(align) complex<Type> thread_local datasub1[maxN];
    alignas(align) complex<Type> thread_local datasub2[maxN];
    alignas(align) complex<Type> thread_local datasub3[maxN];
    alignas(align) complex<Type> thread_local datasub4[maxN];
    Type thread_local a;
    int thread_local PF,NoverPF;
    int thread_local kleft,kright,mleft,nright,tail;
    alignas(align) complex<Type> thread_local roots[maxN];
    alignas(align) int thread_local Factor[100];
    int thread_local NFactor;
    alignas(align) complex<Type> thread_local datatemp[maxN];
    int thread_local kkleft,kkright,mmleft,nnright;
    complex<Type> thread_local *dataptr,*outptr;
    
    i = N & (N - 1);                                                                            //  if 2^, use fft
    if(i == 0) {
        fft_func(data,out,N,Product,pi,sign);
	return;
    }

    roots[0].setrealimga(1.,0.);
    if(sign > 0)
        datasub1[0].setangle(-2.*pi/N);
    else
        datasub1[0].setangle(2.*pi/N);

    if(N%2 == 0) {
        for(i=1;i<N/2;i++) roots[i] = roots[i-1]*datasub1[0];
        for(i=1;i<N/2;i++) roots[N-i].setrealimga(roots[i].getreal(),-roots[i].getimga());
	roots[N/2].setrealimga(-1.,0.);
    } else {
        for(i=1;i<N/2+1;i++) roots[i] = roots[i-1]*datasub1[0];
        for(i=1;i<N/2+1;i++) roots[N-i].setrealimga(roots[i].getreal(),-roots[i].getimga());
    }
    
    NFactor = primeFactors(N,Factor);
    
    for(j=0;j<NFactor;j++) {
        if(j == 0) {
            dataptr = data;
	    if(NFactor%2 == 0) outptr = datatemp; else outptr = out;
        } else if(j == 1) {
            if(NFactor%2 == 0) {
                dataptr = datatemp;
                outptr = out;
            } else {
                dataptr = out;
                outptr = datatemp;
            }
        }
   
        PF = Product*Factor[j];
        NoverPF = N/PF;
        kleft = NoverPF;
        kright = N/Product; 
        mleft = N/Factor[j];                                                 // when m increases by 1, n increases by this
        nright = NoverPF;
        tail = NoverPF;

        if(Factor[j] == 2) {
            p = -NoverPF;
	    kkleft = -kleft;
	    kkright = -kright;
            for(k=0;k<Product;k++) {
                p += NoverPF;
	        kkleft += kleft;
	        kkright += kright;
                for(t=0;t<tail;t++) {
                    datasub2[t] = roots[p]*dataptr[kkright+nright+t];
                    outptr[kkleft+t] = dataptr[kkright+t] + datasub2[t];
		    outptr[kkleft+mleft+t] = dataptr[kkright+t] - datasub2[t]; 
                }
	    }
        } else if(Factor[j] <= maxCooleyTukey) {
            memset(outptr,0,N*sizeof(complex<Type>));
            kkleft = -kleft;
	    kkright = -kright;
	    for(k=0;k<Product;k++) {
	        kkleft += kleft;
	        kkright += kright;
	        nnright = -nright;
	        for(n=0;n<Factor[j];n++) {                                               // summation index
	            nnright += nright;
		    i = n*k*NoverPF;
                    for(t=0;t<tail;t++) {
		        datasub2[t] = roots[i]*dataptr[kkright+nnright+t];
		        outptr[kkleft+0*mleft+t] += datasub2[t];                            // m = 0
		        for(m=1;m<(Factor[j]+1)/2;m++) {
		            datasub1[m] = roots[n*m*mleft%N];
		            datasub3[m] = datasub2[t]*datasub1[m].getreal();
			    datasub4[m] = datasub2[t].turnleft()*datasub1[m].getimga();
			    outptr[kkleft+m*mleft+t] += datasub3[m]+datasub4[m];
			    outptr[kkleft+(Factor[j]-m)*mleft+t] += datasub3[m]-datasub4[m];
		        }
		    }
	        } 
	    }
        } else {
            memset(outptr,0,N*sizeof(complex<Type>));
            kkleft = -kleft;
	    kkright = -kright;
            for(k=0;k<Product;k++) {
	        kkleft += kleft;
	        kkright += kright;
	        nnright = -nright;
	        for(n=0;n<Factor[j];n++) {                                                       //    m=0, summation index
		    p = n*k*NoverPF;
		    nnright += nright;
                    for(t=0;t<tail;t++) 
		        outptr[kkleft+0*mleft+t] += roots[p]*dataptr[kkright+nnright+t];
	        }
	    
	        for(q=1;q<Factor[j];q++)
	            datasub2[q] = roots[q*Product*NoverPF];
	        for(t=0;t<tail;t++) {                                                         //    m=1,.....
		    for(q=1;q<Factor[j];q++)
		        datasub1[q] = roots[q*k*NoverPF]*dataptr[kkright+q*nright+t];
		    Rader<Type>(datasub1,datasub2,datasub3,Factor[j],pi);
	            for(m=1;m<Factor[j];m++) 
		        outptr[kkleft+m*mleft+t] = dataptr[kkright+t] + datasub3[m];	    
	        }
	    }
        }

        if(PF != N) {
            if(j > 0) std::swap(outptr,dataptr);
        } else {
	    if(sign > 0) {
                a = 1./N;
                for(n=0;n<N;n++) out[n] *= a;
	    }
            break;	
        }

        Product = PF;
    }
}

#if fft_bit_reverse != 1
// out-of-place fft
template <class Type>
void fft_func(complex<Type> *data,complex<Type> *out,int N,int Product,Type pi,int sign) {             // if called from main, set sign=1
    int thread_local i,j,k,m,n,p,q,t;
    alignas(align) complex<Type> thread_local datasub1[maxN];
    Type thread_local a;
    int thread_local PF,NoverPF;
    int thread_local kleft,kright,mleft,nright,tail;
    alignas(align) complex<Type> thread_local roots[maxN];
    int thread_local Factor;
    alignas(align) complex<Type> thread_local datatemp[maxN];
    int thread_local kkleft,kkright,mmleft,nnright;
    complex<Type> thread_local *dataptr,*outptr;

    roots[0].setrealimga(1.,0.);
    if(sign > 0)
        datasub1[0].setangle(-2.*pi/N);
    else
        datasub1[0].setangle(2.*pi/N);
	
    k = N>>3;
    j = N>>2;
    for(i=1;i<=k;i++) roots[i] = roots[i-1]*datasub1[0];                      //     1/8 values
    if(sign > 0) {
        for(i=1;i<k;i++) roots[j-i] = roots[i].swap().reverse();              //     values remaining in quadrant 
        for(i=0;i<j;i++) {
            roots[i+j] = roots[i].turnright();
	    roots[i+(j<<1)] = roots[i].reverse();
            roots[i+(j<<2)-j] = roots[i].turnleft();
        }
    } else {
        for(i=1;i<k;i++) roots[j-i] = roots[i].swap();
        for(i=0;i<j;i++) {
            roots[i+j] = roots[i].turnleft();
	    roots[i+(j<<1)] = roots[i].reverse();
            roots[i+(j<<2)-j] = roots[i].turnright();
        }
    }
    
    i = N;                                                       
    j = 0;
    while(i != 1) { i>>=1; j++; }                                // N = 2^j
    j = j%2;
    
    NoverPF = N;
    while(1) {
        if(Product == 1) {
            dataptr = data;
            if(j == 0) outptr = datatemp; else outptr = out;
        } else if(Product == 2) {
            if(j == 0) {
                dataptr = datatemp;
                outptr = out;
            } else {
                dataptr = out;
                outptr = datatemp;
            }
        }
  
        Factor = 2;                                                      
        PF = Product<<1;
        NoverPF >>= 1;
        kleft = NoverPF;
        kright = NoverPF<<1; 
        mleft = N>>1;                                                 // when m increases by 1, n increases by this
        nright = NoverPF;
        tail = NoverPF;

        if(Factor == 2) {
#if AVX == 0
            kkleft = -kleft;
	    p = -NoverPF;
            for(k=0;k<Product;k++) {
	        kkleft += kleft;
	        kkright = kkleft<<1;
                p += NoverPF; 
                for(t=0;t<tail;t++) {
                    datasub1[t] = roots[p]*dataptr[kkright+nright+t];
                    outptr[kkleft+t] = dataptr[kkright+t] + datasub1[t];
	            outptr[kkleft+mleft+t] = dataptr[kkright+t] - datasub1[t]; 
                }
	    }
#else
            if(sizeof(out[0]) == 16) {
                kkleft = -kleft;
	        p = -NoverPF;
                for(k=0;k<Product;k++) {
	            kkleft += kleft;
	            kkright = kkleft<<1;
                    p += NoverPF;
                    if(tail == 1) {
                        datasub1[0] = roots[p]*dataptr[kkright+nright];
                        outptr[kkleft] = dataptr[kkright] + datasub1[0];
                        outptr[kkleft+mleft] = dataptr[kkright] - datasub1[0];
                    } else {
                        for(t=0;t<tail;t+=2) {
                            __m256d a_vals = _mm256_setr_pd(roots[p].getreal(),roots[p].getimga(),roots[p].getreal(),roots[p].getimga());
                            __m256d b_vals = _mm256_load_pd((double *)&dataptr[kkright+nright+t]);
                            __m256d c_vals = _mm256_mul_pd(a_vals,b_vals);
                            c_vals = _mm256_xor_pd(c_vals,_mm256_setr_pd(0.0,-0.0,0.0,-0.0));
                            b_vals = _mm256_permute_pd(b_vals,0b0101);
                            a_vals = _mm256_mul_pd(a_vals,b_vals);
                            b_vals = _mm256_hadd_pd(c_vals,a_vals);                      // complex product
                            a_vals = _mm256_load_pd((double *)&dataptr[kkright+t]);
                            c_vals = _mm256_add_pd(a_vals,b_vals);
                            _mm256_store_pd((double *)&outptr[kkleft+t],c_vals);
                            c_vals = _mm256_sub_pd(a_vals,b_vals);
                            _mm256_store_pd((double *)&outptr[kkleft+mleft+t],c_vals);
                        }
                    }
	        }
            } else if(sizeof(out[0]) == 8) {

            }
#endif
        }

        if(PF != N) {
            if(Product > 1) std::swap(outptr,dataptr);
        } else {
	    if(sign > 0) {
                a = 1./N;
                for(n=0;n<N;n++) out[n] *= a;
	    }
            break;	
        }

        Product = PF;
    }
}
#endif

#if fft_bit_reverse == 1
// in-place fft
template <class Type>
void fft_func(complex<Type> *data,complex<Type> *out,int N,int Product,Type pi,int sign) {             // if called from main, set sign=1
    int thread_local i,j,k,m,n,p,q,h;
    alignas(align) complex<Type> thread_local datasub1[maxN];
    alignas(align) complex<Type> thread_local datasub2[maxN];
    complex<Type> thread_local c1,c2;
    Type thread_local a;
    int thread_local PF,NoverPF;
    int thread_local head,hhead;
    alignas(align) complex<Type> thread_local roots[maxN];
    int thread_local Factor;

    //i = N;
    //j = 0;
    //while(i != 1) { i>>=1; j++; }
    //for(i=0;i<N;i++)
    //    out[table[j][i]] = data[i];
    roots[0].setrealimga(1.,0.);
    if(sign > 0)
        datasub1[0].setangle(-2.*pi/N);
    else
        datasub1[0].setangle(2.*pi/N);

    k = N>>3;
    j = N>>2;
    for(i=1;i<=k;i++) roots[i] = roots[i-1]*datasub1[0];                   //     1/8 values
    if(sign > 0) {
        for(i=1;i<k;i++) roots[j-i] = roots[i].swap().reverse();           //     values remaining in quadrant
        for(i=0;i<j;i++) {
            roots[i+j] = roots[i].turnright();
            roots[i+(j<<1)] = roots[i].reverse();
            roots[i+(j<<2)-j] = roots[i].turnleft();
        }
    } else {
        for(i=1;i<k;i++) roots[j-i] = roots[i].swap();
        for(i=0;i<j;i++) {
            roots[i+j] = roots[i].turnleft();
            roots[i+(j<<1)] = roots[i].reverse();
            roots[i+(j<<2)-j] = roots[i].turnright();
        }
    }


    i = N;
    j = 0;
    while(i != 1) { i>>=1; j++; }
    NoverPF = N;
    while(1) {
        Factor = 2;                                                     
        PF = Product<<1;
        NoverPF >>= 1;
        head = NoverPF;

        if(Factor == 2) {
#if AVX == 0
            hhead = -PF;
            for(h=0;h<head;h++) {
                hhead += PF;
                p = -NoverPF;
                for(k=0;k<Product;k++) {
                    p += NoverPF;
                    c1 = out[hhead+k];
                    c2 = roots[p]*out[hhead+Product+k];
                    out[hhead+k] = c1 + c2;
                    out[hhead+Product+k] = c1 - c2;
                }
            }
#else
            if(sizeof(out[0]) == 16) {                      // double complex
                hhead = -PF;
                for(h=0;h<head;h++) {
                    hhead += PF;
                    p = -NoverPF;
                    if(Product == 1) {
                        //c1 = out[hhead];
                        //c2 = out[hhead+1];
                        c1 = data[table[j][hhead]];
                        c2 = data[table[j][hhead+1]];
                        out[hhead] = c1 + c2;
                        out[hhead+1] = c1 - c2;
                    } else {
                        for(k=0;k<Product;k+=2) {
                            __m256d a_vals = _mm256_setr_pd(roots[k*NoverPF].getreal(),roots[k*NoverPF].getimga(),roots[(k+1)*NoverPF].getreal(),roots[(k+1)*NoverPF].getimga());
                            __m256d b_vals = _mm256_load_pd((double *)&out[hhead+Product+k]);
                            __m256d c_vals = _mm256_mul_pd(a_vals,b_vals);
                            c_vals = _mm256_xor_pd(c_vals,_mm256_setr_pd(0.0,-0.0,0.0,-0.0));
                            b_vals = _mm256_permute_pd(b_vals,0b0101);
                            a_vals = _mm256_mul_pd(a_vals,b_vals);
                            b_vals = _mm256_hadd_pd(c_vals,a_vals);                      // complex product
                            a_vals = _mm256_load_pd((double *)&out[hhead+k]);
                            c_vals = _mm256_add_pd(a_vals,b_vals);
                            _mm256_store_pd((double *)&out[hhead+k],c_vals);
                            c_vals = _mm256_sub_pd(a_vals,b_vals);
                            _mm256_store_pd((double *)&out[hhead+Product+k],c_vals);
                        }
                    }
                }
            } else if(sizeof(out[0]) == 8) {

            }
#endif
        }

        if(PF == N) {
            if(sign > 0) {
                a = 1./N;
                for(n=0;n<N;n++) out[n] *= a;
            }
            break;
        }

        Product = PF;
    }
}
#endif

template <class Type>
void dftinv_func(complex<Type> *data,complex<Type> *out,int N,Type pi) {
    dft_func<Type>(data,out,N,1,pi,-1);
}

template <class Type>
void fftinv_func(complex<Type> *data,complex<Type> *out,int N,Type pi) {
    fft_func<Type>(data,out,N,1,pi,-1);
}

int primeFactors(int N,int *f) {
    int nfactors = 0;
    while(N%2 == 0) {
        N = N>>1;
        f[nfactors] = 2;
        nfactors++;
    }
    for(int i=3;i<=sqrt(N);i+=2) {
        while(N%i == 0) {
            f[nfactors] = i;
            N = N/i;
            nfactors++;
        }
    }
    if(N > 2) {
        f[nfactors] = N;
        nfactors++;
    }
    return nfactors;
}

int powmod(int a,int b,int p) {
    int res = 1;
    while (b)
        if (b&1)
            res = int(res*1ll*a%p), --b;       // 1ll   is long*long
        else
            a = int(a*1ll*a%p), b >>= 1;
    return res;
}

int generator(int p) {
    alignas(align) int fact[100];
    int m=0;
    int phi=p-1, n=phi;
    for (int i=2;i*i<=n;++i)
        if (n % i == 0) {
            fact[m] = i;
            m++;
            while(n%i == 0) n/=i;
        }
    if (n > 1) { fact[m] = n; m++; }

    for (int res=2;res<=p;++res) {
        bool ok = true;
        for (int i=0;i<m && ok;++i) ok &= powmod(res,phi/fact[i],p) != 1;
        if (ok) return res;
    }
    return -1;
}

int gcd1(int a,int b,int &x,int &y) {
    if(b == 0) {
        x = 1;
	y = 0;
	return a;
    }
    int x1, y1, gcd = gcd1(b,a%b,x1,y1);
    x = y1;
    y = x1 - (a/b)*y1;
    return gcd;
}

// p is prime, g is generator
int modulo_inverse(int g,int p) {
    int x,y;
    int z = gcd1(g,p,x,y);
    x = (x%p + p)%p;
    return x;
}

template <class Type>
void Rader(complex<Type> *datasub1,complex<Type> *datasub2,complex<Type> *out,int N,Type pi) {
    int thread_local g,ginv;
    alignas(align) int thread_local mapg[maxN];
    alignas(align) int thread_local mapginv[maxN];
    int thread_local newN;
    int thread_local i;
    alignas(align) complex<Type> thread_local padded1[maxN];
    alignas(align) complex<Type> thread_local padded2[maxN];
    alignas(align) complex<Type> thread_local result1[maxN];
    alignas(align) complex<Type> thread_local result2[maxN];

    g = generator(N);
    ginv = modulo_inverse(g,N);

    mapg[0] = 1;
    for(int q=1;q<N-1;q++) mapg[q] = mapg[q-1]*g%N;                                         // n = g^q    (mod N)  , q=[0,N-2]
    mapginv[0] = 1;
    for(int p=1;p<N-1;p++) mapginv[p] = mapg[N-p-1];                                        // k/m = ginv^p (mod N)  , p=[0,N-2]

    // padding to 2^
    newN = 2;
    for(i=0;i<1000000;i++) {
	if(newN >= 2*(N-1)-1) 
	    break;
        else 
            newN <<= 1;
    }
    
    memset(padded1,0,newN*sizeof(complex<Type>));
    memset(padded2,0,newN*sizeof(complex<Type>));
//    for(int q=0;q<newN;q++) { padded1[q].setzero(); padded2[q].setzero(); }
    for(int q=0;q<N-1;q++) padded1[q] = datasub1[mapg[q]];
    for(int q=0;q<N-1;q++) padded2[q] = datasub2[mapginv[q]];
    for(int q=1;q<N-1;q++) padded2[newN-N+1+q] = padded2[q];
    fft_func<Type>(padded1,result1,newN,1,pi,1);    
    fft_func<Type>(padded2,result2,newN,1,pi,1);
    for(int q=0;q<newN;q++) result1[q] *= result2[q]*newN; 
    fftinv_func<Type>(result1,result2,newN,pi); 
    for(int p=0;p<N-1;p++) out[mapginv[p]] = result2[p];                                     // rearrange    
}


