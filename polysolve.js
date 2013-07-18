/***************************************************************************
 *   Copyright (C) 2012 by Paul Lutus                                      *
 *   lutusp@arachnoid.com                                                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

/* Originally matrix_processor.js.
 * Modified by Devin Papineau, Matt Thiffault, July 2013 */

// a simple data class

function Pair(x,y) {
  this.x = x;
  this.y = y;
};

Pair.prototype.toString = function() {return x + ',' + y};

// a weak substitue for printf()

function number_format(n,p,w) {
  s = n.toExponential(p);
  while(s.length < w) {
    s = ' ' + s;
  }
  return s;
}

// classic Gauss-Jordan matrix manipulation functions

function gj_divide(A,  i,  j,  m) {
  for (var q = j + 1; q < m; q++) {
    A[i][q] /= A[i][j];
  }
  A[i][j] = 1;
}

function gj_eliminate(A, i, j, n, m) {
  for (var k = 0; k < n; k++) {
    if (k != i && A[k][j] != 0) {
      for (var q = j + 1; q < m; q++) {
        A[k][q] -= A[k][j] * A[i][q];
      }
      A[k][j] = 0;
    }
  }
}

function gj_echelonize(A) {
  var n = A.length;
  var m = A[0].length;
  var i = 0;
  var j = 0;
  var k;
  var swap;
  while (i < n && j < m) {
    //look for non-zero entries in col j at or below row i
    k = i;
    while (k < n && A[k][j] == 0) {
      k++;
    }
    // if an entry is found at row k
    if (k < n) {
      //  if k is not i, then swap row i with row k
      if (k != i) {
        swap = A[i];
        A[i] = A[k];
        A[k] = swap;
      }
      // if A[i][j] is != 1, divide row i by A[i][j]
      if (A[i][j] != 1) {
        gj_divide(A, i, j, m);
      }
      // eliminate all other non-zero entries
      gj_eliminate(A, i, j, n, m);
      i++;
    }
    j++;
  }
}

// produce a single y result for a given x

function regress(x, terms) {
  var a = 0;
  var exp = 0;
  for (var i = 0; i < terms.length;i++) {
    term = terms[i];
    a += term * Math.pow(x, exp);
    exp++;
  }
  return a;
}

// compute correlation coefficient

function corr_coeff(data, terms) {
  var r = 0;
  var n = data.length;
  var sx = 0;
  var sx2 = 0, sy = 0, sy2 = 0, sxy = 0;
  var x, y;
  for (var i = 0;i < data.length;i++) {
    pr = data[i];
    var x = regress(pr.x, terms);
    var y = pr.y;
    sx += x;
    sy += y;
    sxy += x * y;
    sx2 += x * x;
    sy2 += y * y;
  }
  var div = Math.sqrt((sx2 - (sx * sx) / n) * (sy2 - (sy * sy) / n));
  if (div != 0) {
    r = Math.pow((sxy - (sx * sy) / n) / div, 2);
  }
  return r;
}

// compute standard error

function std_error( data, terms) {
  var  r = 0;
  var  n = data.length;
  if (n > 2) {
    var a = 0;
    for (var i = 0;i < data.length;i++) {
      pr = data[i];
      a += Math.pow((regress(pr.x, terms) - pr.y), 2);
    }
    r = Math.sqrt(a / (n - 2));
  }
  return r;
}


// create regression coefficients
// for provided data set
// data = pair array
// p = polynomial degree

function compute_coefficients(data,  p) {
  p += 1;
  var n = data.length;
  var r, c;
  var rs = 2 * p - 1;
  //
  // by request: read each datum only once
  // not the most efficient processing method
  // but required if the data set is huge
  //
  // create square matrix with added RH column
  m = Array();
  for (var i = 0; i < p; i++) {
    mm = Array();
    for (var j = 0; j <= p; j++) {
      mm[j] = 0;
    }
    m[i] = mm;
  }
  //double[][] m = new double[p][p + 1];
  // create array of precalculated matrix data
  mpc = Array();
  for(var i = 0;i < rs;i++) {
    mpc[i] = 0;
  }
  mpc[0] = n;
  for (var i = 0;i < data.length;i++) {
    pr = data[i];
    // process precalculation array
    for (r = 1; r < rs; r++) {
      mpc[r] += Math.pow(pr.x, r);
    }
    // process RH column cells
    m[0][p] += pr.y;
    for (r = 1; r < p; r++) {
      m[r][p] += Math.pow(pr.x, r) * pr.y;
    }
  }
  // populate square matrix section
  for (r = 0; r < p; r++) {
    for (c = 0; c < p; c++) {
      m[r][c] = mpc[r + c];
    }
  }
  // reduce matrix
  gj_echelonize(m);
  // extract result column
  terms = Array();
  for (var i = 0;i < m.length;i++) {
    mc = m[i];
    terms[i] = mc[p];
  }
  return terms;
}

// test the system using known data

function test() {
  var xd = [-1,0,1,2,3,5,7,9];
  var yd = [-1,3,2.5,5,4,2,5,4];
  
  data = Array();
  
  for(var i = 0;i < xd.length;i++) {
    data[i] = new Pair(xd[i],yd[i]);
  }
  
  terms = compute_coefficients(data,6);
  
  var prec = 16;
  
  var width = 24;
  
  for(var i = 0;i < terms.length;i++) {
    print(number_format(terms[i],prec,width) + ' * x^' + i);
  }
  
  cc = corr_coeff(data,terms);
  
  print ('cc = ' + number_format(cc,prec,width));
  
  se = std_error(data,terms);
  
  print('se = ' + number_format(se,prec,width));
}

//test();

// "data" is an array of Pair(x,y) data
// p = polynomial degree
function process_data(data,p) {
  var terms = compute_coefficients(data,p);
  var cc = corr_coeff(data,terms);
  var se = std_error(data,terms);
  return [terms,cc,se];
}

/*
 * node.js command-line version! DP, MT
 */

/* adapted from the original polysolve.js */
function parse_data(sd) {
  var xy_data = null;
  var array;
  // try to filter numerical data from an arbitrary input
  var strings = sd.match(/([0-9\.e+-]+)/gim,"$1");
  if(strings != null && strings.length > 0) {
    array = [];
    for(var i in strings) {
      s = strings[i];
      // only if the string contains at least one numerical digit
      if(s.match(/.*[0-9].*/)) {
        array.push(parseFloat(strings[i]));
      }
    }
    var len = array.length;
    if(len % 2 != 0) {
      die("Unpaired data (x count != y count).");
    }
    else {
      //console.debug(array);
      xy_data = new Array();
      for(var i = 0;i < len;i += 2) {
        x = parseFloat(array[i]);
        y = parseFloat(array[i+1]);
        xy_data.push(new Pair(x,y));
      }
    }
  }
  else {
    die("No data.");
  }
  return xy_data;
}

/* Print an error message and exit */
function die(msg) {
    process.stdout.write('error: ' + msg + '\n');
    process.exit(1);
}

/* Read data from stdin. */
function read_data(cb) {
    var data_str = '';
    process.stdin.resume();
    process.stdin.setEncoding('utf8');
    process.stdin.on('data', function(chunk) {
        data_str += chunk;
    });
    process.stdin.on('end', function() {
        cb(parse_data(data_str));
    });
}

/* Argument parsing */
function parse_args() {
    var i       = 2;
    var verbose = false;
    if (process.argv[i] == '-v') {
        verbose = true;
        i++;
    }

    if (process.argv.length != i + 1) {
        die('usage: ' + process.argv[0] + ' ' + process.argv[1] + ' [-v] DEGREE');
    }

    var degree = parseInt(process.argv[i]);
    if (degree != degree || degree <= 0) {
        die('polynomial degree must be a positive integer');
    }

    return {
        degree: degree,
        verbose: verbose
    };
}

/* Main program */
args = parse_args();
read_data(function(data) {
    var result = process_data(data, args.degree);
    var coeffs = result[0];
    var corr   = result[1];
    var stderr = result[2];

    if (args.verbose) {
        process.stdout.write('Correlation coefficient: ' + corr + '\n');
        process.stdout.write('Standard error: ' + stderr + '\n');
        process.stdout.write('Polynomial coefficients:\n');
    }

    coeffs.forEach(function(val, ix, array) {
        process.stdout.write(val + '\n');
    });

    process.exit(0);
});
