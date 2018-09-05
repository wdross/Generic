package net.boxathome.waynes.mysuvat.main;

import android.os.Bundle;
import android.support.design.widget.FloatingActionButton;
import android.support.design.widget.Snackbar;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.Toolbar;
import android.view.View;
import android.view.Menu;
import android.view.MenuItem;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.TextView;
import android.widget.Toast;

public class MainActivity extends AppCompatActivity {

    static int haveDistance=1;
    static int haveInitial=2;
    static int haveFinal=4;
    static int haveAcceleration=8;
    static int haveTime=16;

    // All the "clear" buttons will be assigned to this one click handler
    // When pressed, they simply clear their associated EditText,
    // which is indicated by being the "nextFocusDown" element
    // described in the .xml file
    View.OnClickListener onClickListener = new View.OnClickListener() {

        @Override
        public void onClick(View v) {
            EditText et = findViewById(v.getNextFocusDownId());
            et.setText("", TextView.BufferType.EDITABLE); // wipe out the text
        }
    };


    View.OnClickListener compute = new View.OnClickListener() {

        @Override
        public void onClick(View v) {

            // check to see if exactly two text fields are empty
            // Start with SText and look at that plus the next 4 fields linked via
            // 'nextFocusDown'
            EditText et = findViewById(R.id.SText); // first one
            int empty = 0;
            double[] vals = new double[5];
            int availableParams = 0;
            for (int q=0; q<5; q++) {
                String s = et.getText().toString();
                if (s.isEmpty())
                    empty++;
                else if (s.matches("^[-+]?[0-9]*\\.?[0-9]+$")) {
                    availableParams |= 1 << q;
                    vals[q] = Double.valueOf(s);
/*
                    // field validation?
                    if (et == findViewById(R.id.TText) && vals[q] == 0) {
                        et.setText("Time cannot be zero");
                        return;
                    }
*/
                }
                else {
                  et.setText("", TextView.BufferType.EDITABLE);
                  empty++;
                }
                et = findViewById(et.getNextFocusDownId()); // follow link to next
            }

            if (empty != 2) {
                Toast.makeText(MainActivity.this, "Need exactly 2 empty fields", Toast.LENGTH_LONG).show();
                return;
            }

            double S,U,V,A,T;

            // assign them all, 3 are valid, the others will be calculated:
            S = vals[0];
            U = vals[1];
            V = vals[2];
            A = vals[3];
            T = vals[4];

            // now we can perform our magic, and compute the empty fields based upon
            // the existing fields.
            // The bitfield availableParams has a bit set for each given parameter, so any
            // zero bits means that value needs to be computed.
            // For each zero, there will be four possibilities of which other variables are available
            // to compute a result -- find the first one that has the necessary parameters
            //
            // Formula to choose from at:
            // https://en.wikipedia.org/wiki/Equations_of_motion#Constant_translational_acceleration_in_a_straight_line
            //
            // So we need all 20 different variation solutions
            // We find a table of these on reddit:
            // https://www.reddit.com/r/Physics/comments/7rw6hi/this_table_gives_you_every_suvatxuvat_equation_of/
            //
            // We find a different table at:
            // https://jameskennedymonash.files.wordpress.com/2016/11/suvat-rearranged1.png
            //
            // They are different enough that they could contain errors, so need to get to the
            // bottom of that and do the correct implementation
//            Toast.makeText(MainActivity.this,"OK boss, working on it...", Toast.LENGTH_LONG).show();

            if ((availableParams & haveDistance) == 0) {
                et = findViewById(R.id.SText);
                // compute distance, one of the remaining 4 elements are missing, so don't use that formula
                if ((availableParams & haveInitial) == 0) {
                    // have VAT **
                    S = (V - A * T / 2) * T; // form #5
                }
                else if ((availableParams & haveFinal) == 0) {
                    // have UAT **
                    S = (U + A * T / 2) * T; // form #2
                }
                else if ((availableParams & haveAcceleration) == 0) {
                    // have UVT **
                    S = (U + V) * T / 2; // form #3
                }
                else if ((availableParams & haveTime) == 0) {
                    // have UVA **
                    // only one left that could be missing, do I even need to test??
                    if (A == 0) {
                        et.setText("Acceleration cannot be zero");
                        return;
                    }
                    S = (V*V - U*U) / (2 * A); // solve for S in form #4
                }
                et.setText(Double.toString(S), TextView.BufferType.EDITABLE); // replace the text
            }
            if ((availableParams & haveInitial) == 0) {
                et = findViewById(R.id.UText);
                // compute Initial velocity
                if ((availableParams & haveDistance) == 0) {
                    // have VAT **
                    U = V - A * T; // solve for u in Form #1
                }
                else if ((availableParams & haveFinal) == 0) {
                    // have SAT **
                    if (T == 0) {
                        et.setText("T cannot be zero");
                        return;
                    }
                    U = (S - A * T * T / 2) / T; // solve for U in Form #2
                }
                else if ((availableParams & haveAcceleration) == 0) {
                    // have SVT **
                    if (T == 0) {
                        et.setText("T cannot be zero");
                        return;
                    }
                    U = 2 * S / T - V; // solve for U in Form #3
                }
                else if ((availableParams & haveTime) == 0) {
                    // have SVA **
                    // TODO: determine if +/- is actually possible, how to convey to user
                    U = V * V - 2 * A * S;
                    if (U>0)
                        U = Math.sqrt(U);
                    else {
                        et.setText("cannot compute with given V, A & S");
                        return;
                    }
                }
                et.setText(Double.toString(U), TextView.BufferType.EDITABLE); // replace the text
            }
            if ((availableParams & haveFinal) == 0) {
                et = findViewById(R.id.VText);
                // compute Final velocity
                if ((availableParams & haveDistance) == 0) {
                    // have UAT **
                    V = U + A * T; // form #1
                }
                else if ((availableParams & haveInitial) == 0) {
                    // have SAT **
                    if (T == 0) {
                        et.setText("T cannot be zero");
                        return;
                    }
                    V = S / T + A * T / 2; // solve for V in Form #5
                }
                else if ((availableParams & haveAcceleration) == 0) {
                    // have SUT **
                    if (T == 0) {
                        et.setText("T cannot be zero");
                        return;
                    }
                    V = 2 * S / T - U; // solve for V in Form #3
                }
                else if ((availableParams & haveTime) == 0) {
                    // have SUA **
                    V = U * U + 2 * A * S; // form #4
                    if (V>0)
                        U = Math.sqrt(V);
                    else {
                        et.setText("cannot compute with given S, U & A; 2*A*S is bigger than U*U");
                        return;
                    }
                }
                et.setText(Double.toString(V), TextView.BufferType.EDITABLE); // replace the text
            }
            if ((availableParams & haveAcceleration) == 0) {
                et = findViewById(R.id.AText);
                // compute Acceleration
                if ((availableParams & haveDistance) == 0) {
                    // have UVT **
                    if (T == 0) {
                        et.setText("T cannot be zero");
                        return;
                    }
                    A = (V - U) / T; // solve for a in form #1
                }
                else if ((availableParams & haveInitial) == 0) {
                    // have SVT **
                    if (T == 0) {
                        et.setText("T cannot be zero");
                        return;
                    }
                    A = 2 * (V * T - S) / (T * T); // solve for a in form #5
                }
                else if ((availableParams & haveFinal) == 0) {
                    // have SUT **
                    if (T == 0) {
                        et.setText("T cannot be zero");
                        return;
                    }
                    A = 2 * (S - U * T) / (T * T); // solve for a in form #2
                }
                else if ((availableParams & haveTime) == 0) {
                    // have SUV **
                    A = (V * V - U * U) / (2 * S); // solve for a in form #4
                }
                et.setText(Double.toString(A), TextView.BufferType.EDITABLE); // replace the text
            }
            if ((availableParams & haveTime) == 0) {
                et = findViewById(R.id.TText);
                // compute Time
                if ((availableParams & haveDistance) == 0) {
                    // have UVA **
                    if (A == 0) {
                        et.setText("A cannot be zero");
                        return;
                    }
                    T = (V - U) / A; // solve for t in form #1
                }
                else if ((availableParams & haveInitial) == 0) {
                    // have SVA **
                    // we have the answer from U computed above, so we substitute
                    // that into the same formula and Ta Dah!  Correct answer!
                    if (A == 0) {
                        et.setText("A cannot be zero");
                        return;
                    }
                    T = (V - U) / A; // solve for t in form #1, collecting the answer from U calculated above
                }
                else if ((availableParams & haveFinal) == 0) {
                    // have SUA **
                    if (A == 0) {
                        et.setText("A cannot be zero");
                        return;
                    }
                    T = (V - U) / A; // solve for t in form #1, collecting the answer from V calculated above
                }
                else if ((availableParams & haveAcceleration) == 0) {
                    // have SUV **
                    T = (U + V);
                    if (T == 0) {
                        et.setText("Sum of U + V cannot be zero");
                        return;
                    }
                    T = S * 2 / T; // solve for t in form #3
                }
                et.setText(Double.toString(T), TextView.BufferType.EDITABLE); // replace the text
            }

        }
    };




    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        Toolbar toolbar = (Toolbar) findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);

        // point all buttons to the same code, as which text element
        // to clear is assigned as their "nextFocusDown"
        ImageButton button = findViewById(R.id.clearSButton);
        button.setOnClickListener(onClickListener);
        button = findViewById(R.id.clearUButton);
        button.setOnClickListener(onClickListener);
        button = findViewById(R.id.clearVButton);
        button.setOnClickListener(onClickListener);
        button = findViewById(R.id.clearAButton);
        button.setOnClickListener(onClickListener);
        button = findViewById(R.id.clearTButton);
        button.setOnClickListener(onClickListener);

        FloatingActionButton fab = (FloatingActionButton) findViewById(R.id.fab);
        fab.setOnClickListener(compute);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Inflate the menu; this adds items to the action bar if it is present.
        getMenuInflater().inflate(R.menu.menu_main, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // Handle action bar item clicks here. The action bar will
        // automatically handle clicks on the Home/Up button, so long
        // as you specify a parent activity in AndroidManifest.xml.
        int id = item.getItemId();

        //noinspection SimplifiableIfStatement
        if (id == R.id.action_settings) {
            return true;
        }

        return super.onOptionsItemSelected(item);
    }
}
