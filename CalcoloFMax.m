function F_t_finale=CalcoloFMax(baud,t_c,f_0,toll)
if(t_c <= (1/f_0 - 2/baud))
    % Ciclo per trovare l'upper-bound, deve essere una
    % frequenza che non soddisfi la condizione
    F_t_upper=f_0;
    while(t_c <= (1/F_t_upper - 2/baud))
        F_t_upper = F_t_upper*2;
    end
    F_t_lower = F_t_upper/2; % Ultima frequenza che rispetta la condizione
else
    % Ciclo per trovare il lower-bound, deve essere una
    % frequenza che soddisfi la condizione
    F_t_lower=f_0;
    while(t_c > (1/F_t_lower - 2/baud))
        F_t_lower = F_t_lower/2;
    end
    F_t_upper = F_t_lower*2; % Ultima frequenza che rispetta la condizione
end

for k=1:1000
    delta_F=F_t_upper-F_t_lower;
    F_t_k=F_t_lower+delta_F/2;

    % Se F_t_k soddisfa, sicuramente anche le frequenze minori vanno bene,
    % diventa il lower-bound; se non soddisfa allora sicuramente le
    % frequenze maggiori non vanno bene, diventa l'upper-bound.
    if(t_c <= (1/F_t_k - 2/baud))
        F_t_lower=F_t_k;
    else
        F_t_upper=F_t_k;
    end

    if(F_t_upper - F_t_lower < toll)
        F_t_finale=F_t_lower; % Prendo l'ultima frequenza che rispetta la condizione
        break;
    end
end
end